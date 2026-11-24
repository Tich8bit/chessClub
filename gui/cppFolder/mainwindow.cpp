#include "mainwindow.h"

#include <QAction>
#include <QApplication>
#include <QCalendarWidget>
#include <QComboBox>
#include <QDateEdit>
#include <QDialog>
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QHBoxLayout>
#include <QHeaderView>
#include <QInputDialog>
#include <QLineEdit>
#include <QMenuBar>
#include <QMessageBox>
#include <QPushButton>
#include <QSettings>
#include <QSortFilterProxyModel>
#include <QStatusBar>
#include <QTabWidget>
#include <QTableView>
#include <QTextEdit>
#include <QVBoxLayout>
#include <QWidget>

#include <algorithm>

#include "chestablemodel.h"
#include "chessClub.h"
#include "coach.h"
#include "dialogs.h"
#include "exceptions/exception.h"
#include "files.h"
#include "gamedialog.h"
#include "match.h"
#include "player.h"
#include "specialization.h"
#include "tournament.h"
#include "tournamentSystem.h"
#include "trainingSession.h"

namespace {

QString timeControlName(Tournament::TimeControl control)
{
    switch (control) {
    case Tournament::TimeControl::Blitz:   return "Блиц";
    case Tournament::TimeControl::Rapid:   return "Рапид";
    case Tournament::TimeControl::Classic: return "Классика";
    }
    return "?";
}

QString matchStatusLabel(const Match &match)
{
    const QString rawResult = QString::fromStdString(match.getResult().toString());
    if (match.getResult().isPlayed()) {
        return rawResult.isEmpty() ? QString("Сыграна") : QString("Сыграна (%1)").arg(rawResult);
    }
    return "Не сыграна";
}

constexpr auto STATE_FILE  = "club.txt";
constexpr auto LOG_FILE    = "journal.log";
constexpr auto REPORT_FILE = "report.txt";

QString stateDirectory()
{
    const QStringList candidates = {"club.txt", "../club.txt", "../../club.txt"};
    for (const QString &path : candidates) {
        if (QFileInfo::exists(path)) {
            return QFileInfo(path).absolutePath();
        }
    }
    return QDir::currentPath();
}

QString filePath(const char *name)
{
    return QDir(stateDirectory()).filePath(QString::fromUtf8(name));
}

void createTestClub(ChessClub &club)
{
    auto p1 = std::make_shared<Player>(1, "Иван", "Иванов", "Иванович", Ratings(1200, 1250, 1300), 0, 0, 0);
    auto p2 = std::make_shared<Player>(2, "Пётр", "Петров", "Петрович", Ratings(1800, 1750, 1700), 0, 0, 0);
    auto c1 = std::make_shared<Coach>(3, "Тимофей", "Тренеров", "Тимофеевич",
                                      Ratings(2000, 1950, 1900), 0, 0, 0, 25,
                                      Specialization(Specialization::Type::Openings), 4);

    club.addPlayer(p1);
    club.addPlayer(p2);
    club.addPlayer(c1);

    auto system = std::make_shared<RoundRobinSystem>(1, true);
    auto tournament = std::make_shared<Tournament>(
        1, "Чемпионат", Tournament::TimeControl::Classic, system, 12);
    *tournament += p1;
    *tournament += p2;
    club.addEvent(tournament);

    auto training = std::make_shared<TrainingSession>(
        2, "Тактика и эндшпиль", "Связанные проходные", 90, c1);
    *training += p1;
    club.addEvent(training);

    auto match = std::make_shared<Match>(1, p1, p2, DateTime(1, 11, 2026, 14, 30, 0));
    match->setTournamentId(1);
    *tournament += match;
    club.addMatch(match);
}

} // namespace

MainWindow::MainWindow(const QString &clubName, QWidget *parent)
    : QMainWindow(parent)
    , _club(std::make_unique<ChessClub>(clubName.toStdString()))
{
    setWindowTitle(clubName.isEmpty() ? QStringLiteral("chessClub") : clubName);
    resize(1100, 680);

    buildUi();

    QSettings settings("chessClub", "chessClubGui");
    applyTheme(settings.value("theme", "Светлая").toString());

    refreshAll();

    statusBar()->showMessage(QString("Клуб: %1 · участников: %2")
                                 .arg(QString::fromStdString(std::string(_club->getName())))
                                 .arg(_club->getPlayerCount()));
}

MainWindow::MainWindow(QWidget *parent)
    : MainWindow(QStringLiteral("Гамбит"), parent)
{
}

MainWindow::~MainWindow() = default;

int MainWindow::startupChoice()
{
    QMessageBox box;
    box.setWindowTitle("chessClub");
    box.setText("Как начать работу?");
    box.setInformativeText("Тестовая система — готовые демо-данные.\n"
                           "Загрузка — прочитать club.txt.\n"
                           "Новый клуб — начать с пустого списка.");

    auto *testButton = box.addButton("Тестовая система", QMessageBox::AcceptRole);
    auto *loadButton = box.addButton("Загрузить club.txt", QMessageBox::ActionRole);
    auto *newButton = box.addButton("Новый клуб", QMessageBox::RejectRole);
    box.exec();

    if (box.clickedButton() == testButton) return 2;
    if (box.clickedButton() == loadButton) return 1;
    if (box.clickedButton() == newButton) return 0;

    return -1;
}

void MainWindow::applyStartup(int mode)
{
    if (mode == 1) {
        try {
            auto loaded = std::make_unique<ChessClub>(std::string(_club->getName()));
            StorageManager::loadState(filePath(STATE_FILE).toStdString(), *loaded);
            _club = std::move(loaded);
        } catch (const ChessException &e) {
            QMessageBox::warning(this, "Загрузка", e.what());
        }
    } else if (mode == 2) {
        createTestClub(*_club);
    }

    refreshAll();
}

void MainWindow::buildUi()
{
    auto *fileMenu = menuBar()->addMenu("Файл");
    fileMenu->addAction("Сохранить", this, &MainWindow::saveState);
    fileMenu->addAction("Отчёт", this, &MainWindow::makeReport);
    fileMenu->addSeparator();
    fileMenu->addAction("Выход", this, &QWidget::close);

    auto *adminMenu = menuBar()->addMenu("Админ");
    adminMenu->addAction("Открыть отчёт", this, &MainWindow::viewReport);
    adminMenu->addAction("Открыть журнал (логи)", this, &MainWindow::viewLog);

    auto *viewMenu = menuBar()->addMenu("Вид");
    auto *themeMenu = viewMenu->addMenu("Тема");
    const char *themes[] = {"Светлая", "Тёмная", "Синяя"};
    for (const char *name : themes) {
        QAction *action = themeMenu->addAction(QString::fromUtf8(name));
        connect(action, &QAction::triggered, this, [this, name]() {
            applyTheme(QString::fromUtf8(name));
        });
    }

    _tabs = new QTabWidget(this);
    setCentralWidget(_tabs);

    {
        auto *page = buildTableTab(_playersTab,
            {"ID", "ФИО", "Классика", "Рапид", "Блиц", "В", "Н", "П"});

        auto *buttons = new QHBoxLayout;
        auto *add = new QPushButton("Добавить", page);
        auto *remove = new QPushButton("Удалить", page);
        auto *history = new QPushButton("История матчей", page);
        buttons->addWidget(add);
        buttons->addWidget(remove);
        buttons->addWidget(history);
        buttons->addStretch();

        qobject_cast<QVBoxLayout *>(page->layout())->addLayout(buttons);
        connect(add, &QPushButton::clicked, this, &MainWindow::addPlayer);
        connect(remove, &QPushButton::clicked, this, &MainWindow::deletePlayer);
        connect(history, &QPushButton::clicked, this, &MainWindow::showPlayerHistory);

        _tabs->addTab(page, "Игроки");
    }

    {
        auto *page = buildTableTab(_coachesTab,
            {"ID", "ФИО", "Классика", "Рапид", "Блиц", "Стаж", "Специализация", "Квалификация"});

        auto *buttons = new QHBoxLayout;
        auto *add = new QPushButton("Добавить", page);
        auto *remove = new QPushButton("Удалить", page);
        buttons->addWidget(add);
        buttons->addWidget(remove);
        buttons->addStretch();

        qobject_cast<QVBoxLayout *>(page->layout())->addLayout(buttons);
        connect(add, &QPushButton::clicked, this, &MainWindow::addCoach);
        connect(remove, &QPushButton::clicked, this, &MainWindow::deleteCoach);

        _tabs->addTab(page, "Тренеры");
    }

    {
        auto *page = buildTableTab(_tournamentsTab,
            {"ID", "Название", "Контроль", "Система", "Участники", "Лимит", "Партии"});

        auto *buttons = new QHBoxLayout;
        auto *add = new QPushButton("Добавить", page);
        auto *remove = new QPushButton("Удалить", page);
        auto *enroll = new QPushButton("Записать игрока", page);
        auto *withdraw = new QPushButton("Снять", page);
        auto *standings = new QPushButton("Таблица", page);
        buttons->addWidget(add);
        buttons->addWidget(remove);
        buttons->addWidget(enroll);
        buttons->addWidget(withdraw);
        buttons->addWidget(standings);
        buttons->addStretch();

        qobject_cast<QVBoxLayout *>(page->layout())->addLayout(buttons);
        connect(add, &QPushButton::clicked, this, &MainWindow::addTournament);
        connect(remove, &QPushButton::clicked, this, &MainWindow::deleteTournament);
        connect(enroll, &QPushButton::clicked, this, &MainWindow::enrollPlayer);
        connect(withdraw, &QPushButton::clicked, this, &MainWindow::withdrawPlayer);
        connect(standings, &QPushButton::clicked, this, &MainWindow::showStandings);

        _tabs->addTab(page, "Турниры");
    }

    {
        auto *page = buildTableTab(_trainingsTab,
            {"ID", "Название", "Тема", "Тренер", "Длительность (мин)", "Участники"});

        auto *buttons = new QHBoxLayout;
        auto *add = new QPushButton("Добавить", page);
        auto *remove = new QPushButton("Удалить", page);
        auto *enroll = new QPushButton("Записать участника", page);
        auto *withdraw = new QPushButton("Снять", page);
        auto *details = new QPushButton("Состав и детали", page);
        buttons->addWidget(add);
        buttons->addWidget(remove);
        buttons->addWidget(enroll);
        buttons->addWidget(withdraw);
        buttons->addWidget(details);
        buttons->addStretch();

        qobject_cast<QVBoxLayout *>(page->layout())->addLayout(buttons);
        connect(add, &QPushButton::clicked, this, &MainWindow::addTraining);
        connect(remove, &QPushButton::clicked, this, &MainWindow::deleteTraining);
        connect(enroll, &QPushButton::clicked, this, &MainWindow::enrollTrainingPlayer);
        connect(withdraw, &QPushButton::clicked, this, &MainWindow::withdrawTrainingPlayer);
        connect(details, &QPushButton::clicked, this, &MainWindow::showTrainingDetails);

        _tabs->addTab(page, "Тренировки");
    }

    {
        auto *page = buildTableTab(_matchesTab,
            {"ID", "Белые", "Чёрные", "Дата", "Статус", "Турнир"}, true);

        auto *buttons = new QHBoxLayout;
        auto *add = new QPushButton("Создать", page);
        auto *play = new QPushButton("Играть", page);
        auto *result = new QPushButton("Результат", page);
        auto *remove = new QPushButton("Удалить", page);
        buttons->addWidget(add);
        buttons->addWidget(play);
        buttons->addWidget(result);
        buttons->addWidget(remove);
        buttons->addStretch();

        qobject_cast<QVBoxLayout *>(page->layout())->addLayout(buttons);
        connect(add, &QPushButton::clicked, this, &MainWindow::addMatch);
        connect(play, &QPushButton::clicked, this, &MainWindow::playMatch);
        connect(result, &QPushButton::clicked, this, &MainWindow::setMatchResult);
        connect(remove, &QPushButton::clicked, this, &MainWindow::deleteMatch);

        _tabs->addTab(page, "Матчи");
    }
}

QWidget *MainWindow::buildTableTab(Tab &tab, const QStringList &headers, bool withMatchStatusFilter)
{
    Q_UNUSED(headers);

    tab.model = new ChessTableModel(this);
    tab.proxy = new QSortFilterProxyModel(this);
    tab.proxy->setSourceModel(tab.model);
    tab.proxy->setSortRole(ChessTableModel::SortRole);
    tab.proxy->setFilterCaseSensitivity(Qt::CaseInsensitive);
    tab.proxy->setFilterKeyColumn(-1);

    tab.view = new QTableView;
    tab.view->setModel(tab.proxy);
    tab.view->setSortingEnabled(true);
    tab.view->setSelectionBehavior(QAbstractItemView::SelectRows);
    tab.view->setSelectionMode(QAbstractItemView::SingleSelection);
    tab.view->setEditTriggers(QAbstractItemView::NoEditTriggers);
    tab.view->verticalHeader()->setVisible(false);
    tab.view->horizontalHeader()->setStretchLastSection(true);

    tab.filter = new QLineEdit;
    tab.filter->setPlaceholderText("Фильтр по таблице...");

    auto *page = new QWidget;
    auto *layout = new QVBoxLayout(page);
    layout->addWidget(tab.filter);

    if (withMatchStatusFilter) {
        auto *filterRow = new QHBoxLayout;
        tab.statusFilter = new QComboBox(page);
        tab.statusFilter->addItem("Все матчи", 0);
        tab.statusFilter->addItem("Несыгранные", 1);
        tab.statusFilter->addItem("Сыгранные", 2);
        filterRow->addWidget(tab.statusFilter);
        filterRow->addStretch();
        layout->addLayout(filterRow);
        connect(tab.statusFilter, &QComboBox::currentIndexChanged, this, [this]() { refreshMatches(); });
    }

    layout->addWidget(tab.view, 1);

    Tab *tabPtr = &tab;
    connect(tab.filter, &QLineEdit::textChanged, this, [tabPtr](const QString &text) {
        tabPtr->proxy->setFilterFixedString(text);
    });

    connect(tab.view->horizontalHeader(), &QHeaderView::sectionDoubleClicked, this,
            [tabPtr](int column) { tabPtr->view->resizeColumnToContents(column); });

    return page;
}

void MainWindow::openTextFileDialog(const QString &title, const QString &path)
{
    QFile file(path);
    if (!file.exists()) {
        message(QString("Файл не найден:\n%1").arg(QDir::toNativeSeparators(path)));
        return;
    }

    if (!file.open(QIODevice::ReadOnly | QIODevice::Text)) {
        QMessageBox::warning(this, "Ошибка чтения",
                             QString("Не удалось открыть файл:\n%1").arg(QDir::toNativeSeparators(path)));
        return;
    }

    auto *dialog = new QDialog(this);
    dialog->setWindowTitle(title);
    dialog->resize(760, 520);

    auto *layout = new QVBoxLayout(dialog);
    auto *viewer = new QTextEdit(dialog);
    viewer->setReadOnly(true);
    viewer->setLineWrapMode(QTextEdit::NoWrap);
    viewer->setPlainText(QString::fromUtf8(file.readAll()));
    file.close();

    auto *buttons = new QHBoxLayout;
    auto *refreshBtn = new QPushButton("Обновить", dialog);
    auto *closeBtn = new QPushButton("Закрыть", dialog);
    buttons->addWidget(refreshBtn);
    buttons->addStretch();
    buttons->addWidget(closeBtn);

    layout->addWidget(viewer, 1);
    layout->addLayout(buttons);

    connect(refreshBtn, &QPushButton::clicked, dialog, [dialog, viewer, path]() {
        QFile f(path);
        if (f.open(QIODevice::ReadOnly | QIODevice::Text)) {
            viewer->setPlainText(QString::fromUtf8(f.readAll()));
            f.close();
        } else {
            QMessageBox::warning(dialog, "Ошибка чтения", "Не удалось перечитать файл.");
        }
    });
    connect(closeBtn, &QPushButton::clicked, dialog, &QDialog::accept);

    dialog->exec();
    dialog->deleteLater();
}

void MainWindow::refreshAll()
{
    refreshPlayers();
    refreshCoaches();
    refreshTournaments();
    refreshTrainings();
    refreshMatches();
}

void MainWindow::refreshPlayers()
{
    QVector<ChessTableModel::Row> rows;

    for (const auto &member : _club->getPlayers()) {
        auto player = std::dynamic_pointer_cast<Player>(member);
        if (!player) {
            continue;
        }

        ChessTableModel::Row row;
        row.id = player->getId();
        row.muted = false;
        row.text = {
            QString::number(player->getId()),
            QString::fromStdString(player->getFullName()),
            QString::number(player->getRatings().getClassical()),
            QString::number(player->getRatings().getRapid()),
            QString::number(player->getRatings().getBlitz()),
            QString::number(player->getWins()),
            QString::number(player->getDraws()),
            QString::number(player->getLosses())
        };
        row.values = {
            player->getId(),
            QString::fromStdString(player->getFullName()),
            player->getRatings().getClassical(),
            player->getRatings().getRapid(),
            player->getRatings().getBlitz(),
            player->getWins(),
            player->getDraws(),
            player->getLosses()
        };
        rows.push_back(row);
    }

    _playersTab.model->resetRows(
        {"ID", "ФИО", "Классика", "Рапид", "Блиц", "В", "Н", "П"}, rows);
}

void MainWindow::refreshCoaches()
{
    QVector<ChessTableModel::Row> rows;

    for (const auto &member : _club->getPlayers()) {
        auto coach = std::dynamic_pointer_cast<Coach>(member);
        if (!coach) {
            continue;
        }

        ChessTableModel::Row row;
        row.id = coach->getId();
        row.muted = false;
        row.text = {
            QString::number(coach->getId()),
            QString::fromStdString(coach->getFullName()),
            QString::number(coach->getRatings().getClassical()),
            QString::number(coach->getRatings().getRapid()),
            QString::number(coach->getRatings().getBlitz()),
            QString::number(coach->getExperienceYears()),
            QString::fromStdString(coach->getSpecialization().toString()),
            QString::number(coach->getQualificationLevel())
        };
        row.values = {
            coach->getId(),
            QString::fromStdString(coach->getFullName()),
            coach->getRatings().getClassical(),
            coach->getRatings().getRapid(),
            coach->getRatings().getBlitz(),
            coach->getExperienceYears(),
            QString::fromStdString(coach->getSpecialization().toString()),
            coach->getQualificationLevel()
        };
        rows.push_back(row);
    }

    _coachesTab.model->resetRows(
        {"ID", "ФИО", "Классика", "Рапид", "Блиц", "Стаж", "Специализация", "Квалификация"}, rows);
}

void MainWindow::refreshTournaments()
{
    QVector<ChessTableModel::Row> rows;

    for (const auto &event : _club->getEvents()) {
        auto tournament = std::dynamic_pointer_cast<Tournament>(event);
        if (!tournament) {
            continue;
        }

        ChessTableModel::Row row;
        row.id = tournament->getId();
        row.text = {
            QString::number(tournament->getId()),
            QString::fromStdString(std::string(tournament->getTitle())),
            timeControlName(tournament->getTimeControl()),
            QString::fromStdString(tournament->getSystem()->getName()),
            QString::number(tournament->getParticipants().size()),
            QString::number(tournament->getMaxPlayers()),
            QString::number(tournament->getMatches().size())
        };
        row.values = {
            tournament->getId(),
            QString::fromStdString(std::string(tournament->getTitle())),
            timeControlName(tournament->getTimeControl()),
            QString::fromStdString(tournament->getSystem()->getName()),
            static_cast<int>(tournament->getParticipants().size()),
            tournament->getMaxPlayers(),
            static_cast<int>(tournament->getMatches().size())
        };
        rows.push_back(row);
    }

    _tournamentsTab.model->resetRows(
        {"ID", "Название", "Контроль", "Система", "Участники", "Лимит", "Партии"}, rows);
}

void MainWindow::refreshTrainings()
{
    QVector<ChessTableModel::Row> rows;

    for (const auto &event : _club->getEvents()) {
        auto training = std::dynamic_pointer_cast<TrainingSession>(event);
        if (!training) {
            continue;
        }

        const QString coachName = training->getCoach()
            ? QString::fromStdString(training->getCoach()->getFullName())
            : QString("—");

        ChessTableModel::Row row;
        row.id = training->getId();
        row.text = {
            QString::number(training->getId()),
            QString::fromStdString(std::string(training->getTitle())),
            QString::fromStdString(std::string(training->getTopic())),
            coachName,
            QString::number(training->getDurationMinutes()),
            QString::number(training->getParticipants().size())
        };
        row.values = {
            training->getId(),
            QString::fromStdString(std::string(training->getTitle())),
            QString::fromStdString(std::string(training->getTopic())),
            coachName,
            training->getDurationMinutes(),
            static_cast<int>(training->getParticipants().size())
        };
        rows.push_back(row);
    }

    _trainingsTab.model->resetRows(
        {"ID", "Название", "Тема", "Тренер", "Длительность (мин)", "Участники"}, rows);
}

void MainWindow::refreshMatches()
{
    QVector<ChessTableModel::Row> rows;
    const int filterMode = _matchesTab.statusFilter ? _matchesTab.statusFilter->currentData().toInt() : 0;

    for (const auto &match : _club->getMatches()) {
        const bool played = match->getResult().isPlayed();
        if (filterMode == 1 && played) {
            continue;
        }
        if (filterMode == 2 && !played) {
            continue;
        }

        const QString statusStr = matchStatusLabel(*match);

        ChessTableModel::Row row;
        row.id = match->getId();
        row.text = {
            QString::number(match->getId()),
            QString::fromStdString(match->getWhitePlayer()->getFullName()),
            QString::fromStdString(match->getBlackPlayer()->getFullName()),
            QString::fromStdString(match->getDate().toString()),
            statusStr,
            match->getTournamentId() > 0 ? QString::number(match->getTournamentId()) : QString("—")
        };
        row.values = {
            match->getId(),
            QString::fromStdString(match->getWhitePlayer()->getFullName()),
            QString::fromStdString(match->getBlackPlayer()->getFullName()),
            QString::fromStdString(match->getDate().toString()),
            statusStr,
            match->getTournamentId()
        };
        rows.push_back(row);
    }

    _matchesTab.model->resetRows(
        {"ID", "Белые", "Чёрные", "Дата", "Статус", "Турнир"}, rows);
}

int MainWindow::selectedId(const Tab &tab) const
{
    if (!tab.view->selectionModel()) {
        return -1;
    }

    const QModelIndex current = tab.view->selectionModel()->currentIndex();
    if (!current.isValid()) {
        return -1;
    }

    const QModelIndex source = tab.proxy->mapToSource(current);
    return tab.model->idAt(source.row());
}

void MainWindow::logAction(const QString &action)
{
    try {
        StorageManager::logAction(filePath(LOG_FILE).toStdString(), action.toStdString());
    } catch (const ChessException &) {
    }
}

void MainWindow::message(const QString &text)
{
    QMessageBox::information(this, "chessClub", text);
}

void MainWindow::addPlayer()
{
    AddPlayerDialog dialog(this);
    if (dialog.exec() != QDialog::Accepted) {
        return;
    }

    try {
        const int id = _club->nextPlayerId();
        _club->addPlayer(std::make_shared<Player>(
            id, dialog.firstName.toStdString(), dialog.lastName.toStdString(),
            dialog.middleName.toStdString(),
            Ratings(dialog.classical, dialog.rapid, dialog.blitz), 0, 0, 0));

        logAction(QString("добавлен игрок ID=%1").arg(id));
        refreshAll();
        statusBar()->showMessage(QString("Игрок добавлен, ID %1").arg(id), 4000);
    } catch (const ChessException &e) {
        QMessageBox::warning(this, "Ошибка", e.what());
    }
}

void MainWindow::showPlayerHistory()
{
    const int id = selectedId(_playersTab);
    if (id < 0) {
        message("Выберите игрока в таблице.");
        return;
    }

    auto player = std::dynamic_pointer_cast<Player>(_club->findPlayerById(id));
    if (!player) {
        message("Это не игрок.");
        return;
    }

    const auto matches = _club->matchesOfPlayer(id);

    QString header = QString("%1 (ID %2)\n"
                             "В профиле: В/Н/П = %3/%4/%5\n"
                             "Партий в базе: %6\n\n")
                         .arg(QString::fromStdString(player->getFullName()))
                         .arg(id)
                         .arg(player->getWins())
                         .arg(player->getDraws())
                         .arg(player->getLosses())
                         .arg(matches.size());

    if (matches.empty()) {
        QMessageBox::information(this, "История матчей",
                                 header + "В базе нет ни одной партии этого игрока.\n"
                                          "Показатели В/Н/П взяты из сохранённых данных "
                                          "(они могли быть внесены вручную или в старой версии программы).");
        return;
    }

    QString body;
    for (const auto &match : matches) {
        const bool isWhite = match->getWhitePlayer()->getId() == id;
        auto opponent = isWhite ? match->getBlackPlayer() : match->getWhitePlayer();
        const auto result = match->getResult();

        QString outcome = "не сыграна";
        if (result.isPlayed()) {
            const double score = isWhite ? result.getWhiteScore() : result.getBlackScore();
            if (score >= 1.0) outcome = "победа";
            else if (score > 0.0) outcome = "ничья";
            else outcome = "поражение";
        }

        QString tournamentLabel;
        if (match->getTournamentId() > 0) {
            if (auto tournament = _club->findTournamentById(match->getTournamentId())) {
                tournamentLabel = QString(" · турнир: %1")
                                      .arg(QString::fromStdString(std::string(tournament->getTitle())));
            } else {
                tournamentLabel = QString(" · турнир %1").arg(match->getTournamentId());
            }
        }

        body += QString("%1 · %2 · соперник: %3 · %4%5\n")
                    .arg(QString::fromStdString(match->getDate().toString()))
                    .arg(isWhite ? "белые" : "чёрные")
                    .arg(QString::fromStdString(opponent->getFullName()))
                    .arg(outcome)
                    .arg(tournamentLabel);
    }

    QMessageBox::information(this, "История матчей", header + body);
}

void MainWindow::deletePlayer()
{
    const int id = selectedId(_playersTab);
    if (id < 0) {
        message("Выберите игрока в таблице.");
        return;
    }

    if (QMessageBox::question(this, "Удаление",
                              QString("Удалить участника ID %1? Его партии тоже будут удалены.").arg(id))
        != QMessageBox::Yes) {
        return;
    }

    if (_club->removePlayer(id)) {
        logAction(QString("удалён участник ID=%1").arg(id));
        refreshAll();
        statusBar()->showMessage("Удалено.", 4000);
    } else {
        message("Участник не найден.");
    }
}

void MainWindow::addCoach()
{
    AddCoachDialog dialog(this);
    if (dialog.exec() != QDialog::Accepted) {
        return;
    }

    try {
        const int id = _club->nextPlayerId();
        _club->addPlayer(std::make_shared<Coach>(
            id, dialog.firstName.toStdString(), dialog.lastName.toStdString(),
            dialog.middleName.toStdString(),
            Ratings(dialog.classical, dialog.rapid, dialog.blitz), 0, 0, 0,
            dialog.experience,
            Specialization(Specialization::intToType(dialog.specialization)),
            dialog.qualification));

        logAction(QString("добавлен тренер ID=%1").arg(id));
        refreshAll();
        statusBar()->showMessage(QString("Тренер добавлен, ID %1").arg(id), 4000);
    } catch (const ChessException &e) {
        QMessageBox::warning(this, "Ошибка", e.what());
    }
}

void MainWindow::deleteCoach()
{
    const int id = selectedId(_coachesTab);
    if (id < 0) {
        message("Выберите тренера в таблице.");
        return;
    }

    if (QMessageBox::question(this, "Удаление", QString("Удалить тренера ID %1?").arg(id))
        != QMessageBox::Yes) {
        return;
    }

    if (_club->removePlayer(id)) {
        logAction(QString("удалён тренер ID=%1").arg(id));
        refreshAll();
        statusBar()->showMessage("Удалено.", 4000);
    } else {
        message("Тренер не найден.");
    }
}

void MainWindow::addTournament()
{
    AddTournamentDialog dialog(this);
    if (dialog.exec() != QDialog::Accepted) {
        return;
    }

    std::shared_ptr<TournamentSystem> system;
    if (dialog.system == 0) {
        system = std::make_shared<RoundRobinSystem>(1, true);
    } else if (dialog.system == 1) {
        system = std::make_shared<SwissSystem>(5, true);
    } else {
        system = std::make_shared<KnockoutSystem>(false);
    }

    try {
        const int id = _club->nextEventId();
        _club->addEvent(std::make_shared<Tournament>(
            id, dialog.title.toStdString(),
            static_cast<Tournament::TimeControl>(dialog.timeControl),
            system, dialog.maxPlayers));

        logAction(QString("добавлен турнир ID=%1").arg(id));
        refreshAll();
        statusBar()->showMessage(QString("Турнир добавлен, ID %1").arg(id), 4000);
    } catch (const ChessException &e) {
        QMessageBox::warning(this, "Ошибка", e.what());
    }
}

void MainWindow::deleteTournament()
{
    const int id = selectedId(_tournamentsTab);
    if (id < 0) {
        message("Выберите турнир в таблице.");
        return;
    }

    if (QMessageBox::question(this, "Удаление",
                              QString("Удалить турнир ID %1 вместе с его партиями?").arg(id))
        != QMessageBox::Yes) {
        return;
    }

    if (_club->removeEvent(id)) {
        logAction(QString("удалён турнир ID=%1").arg(id));
        refreshAll();
        statusBar()->showMessage("Удалено.", 4000);
    } else {
        message("Турнир не найден.");
    }
}

void MainWindow::enrollPlayer()
{
    const int tournamentId = selectedId(_tournamentsTab);
    if (tournamentId < 0) {
        message("Выберите турнир в таблице.");
        return;
    }

    bool ok = false;
    const int playerId = QInputDialog::getInt(this, "Запись на турнир",
                                              "ID игрока:", 1, 1, 99999, 1, &ok);
    if (!ok) {
        return;
    }

    if (_club->enrollPlayer(tournamentId, playerId)) {
        logAction(QString("запись игрока %1 на турнир %2").arg(playerId).arg(tournamentId));
        refreshTournaments();
        statusBar()->showMessage("Игрок записан.", 4000);
    } else {
        message("Не удалось: нет игрока, лимит участников или уже записан.");
    }
}

void MainWindow::withdrawPlayer()
{
    const int tournamentId = selectedId(_tournamentsTab);
    if (tournamentId < 0) {
        message("Выберите турнир в таблице.");
        return;
    }

    bool ok = false;
    const int playerId = QInputDialog::getInt(this, "Снятие с турнира",
                                              "ID игрока:", 1, 1, 99999, 1, &ok);
    if (!ok) {
        return;
    }

    if (_club->withdrawPlayer(tournamentId, playerId)) {
        logAction(QString("снятие игрока %1 с турнира %2").arg(playerId).arg(tournamentId));
        refreshAll();
        statusBar()->showMessage("Игрок снят.", 4000);
    } else {
        message("Не удалось: игрок не был записан.");
    }
}

void MainWindow::showStandings()
{
    const int tournamentId = selectedId(_tournamentsTab);
    if (tournamentId < 0) {
        message("Выберите турнир в таблице.");
        return;
    }

    auto tournament = _club->findTournamentById(tournamentId);
    if (!tournament) {
        message("Турнир не найден.");
        return;
    }

    const auto standings = tournament->getStandings(true);
    QString text = QString("Турнир: %1\n\n").arg(QString::fromStdString(std::string(tournament->getTitle())));

    if (standings.empty()) {
        text += "Нет участников.";
    } else {
        for (size_t i = 0; i < standings.size(); ++i) {
            text += QString("%1. %2 — очки: %3 (В/Н/П: %4/%5/%6)\n")
                        .arg(i + 1)
                        .arg(QString::fromStdString(standings[i].player->getFullName()))
                        .arg(standings[i].points)
                        .arg(standings[i].wins)
                        .arg(standings[i].draws)
                        .arg(standings[i].losses);
        }
    }

    QMessageBox::information(this, "Турнирная таблица", text);
}

void MainWindow::addTraining()
{
    bool ok = false;
    const QString title = QInputDialog::getText(
        this, "Новая тренировка", "Название тренировки:", QLineEdit::Normal, QString(), &ok);
    if (!ok || title.trimmed().isEmpty()) {
        return;
    }

    const QString topic = QInputDialog::getText(
        this, "Новая тренировка", "Тема тренировки:", QLineEdit::Normal, "Общая практика", &ok);
    if (!ok) {
        return;
    }

    const int duration = QInputDialog::getInt(
        this, "Новая тренировка", "Длительность (минут):", 90, 15, 480, 15, &ok);
    if (!ok) {
        return;
    }

    const int coachId = QInputDialog::getInt(
        this, "Новая тренировка", "ID тренера:", 1, 1, 99999, 1, &ok);
    if (!ok) {
        return;
    }

    auto coach = std::dynamic_pointer_cast<Coach>(_club->findPlayerById(coachId));
    if (!coach) {
        message("Тренер с таким ID не найден.");
        return;
    }

    try {
        const int id = _club->nextEventId();
        _club->addEvent(std::make_shared<TrainingSession>(
            id, title.trimmed().toStdString(), topic.trimmed().toStdString(),
            duration, coach));

        logAction(QString("добавлена тренировка ID=%1").arg(id));
        refreshTrainings();
        statusBar()->showMessage(QString("Тренировка добавлена, ID %1").arg(id), 4000);
    } catch (const ChessException &e) {
        QMessageBox::warning(this, "Ошибка", e.what());
    }
}

void MainWindow::deleteTraining()
{
    const int id = selectedId(_trainingsTab);
    if (id < 0) {
        message("Выберите тренировку в таблице.");
        return;
    }

    if (QMessageBox::question(this, "Удаление", QString("Удалить тренировку ID %1?").arg(id))
        != QMessageBox::Yes) {
        return;
    }

    if (_club->removeEvent(id)) {
        logAction(QString("удалена тренировка ID=%1").arg(id));
        refreshTrainings();
        statusBar()->showMessage("Удалено.", 4000);
    } else {
        message("Тренировка не найдена.");
    }
}

void MainWindow::enrollTrainingPlayer()
{
    const int trainingId = selectedId(_trainingsTab);
    if (trainingId < 0) {
        message("Выберите тренировку в таблице.");
        return;
    }

    bool ok = false;
    const int playerId = QInputDialog::getInt(
        this, "Запись на тренировку", "ID участника:", 1, 1, 99999, 1, &ok);
    if (!ok) {
        return;
    }

    if (_club->enrollPlayer(trainingId, playerId)) {
        logAction(QString("запись участника %1 на тренировку %2").arg(playerId).arg(trainingId));
        refreshTrainings();
        statusBar()->showMessage("Участник записан на тренировку.", 4000);
        return;
    }

    std::shared_ptr<TrainingSession> training;
    for (const auto &event : _club->getEvents()) {
        auto candidate = std::dynamic_pointer_cast<TrainingSession>(event);
        if (candidate && candidate->getId() == trainingId) {
            training = candidate;
            break;
        }
    }

    auto member = _club->findPlayerById(playerId);
    if (training && member) {
        *training += member;
        logAction(QString("запись участника %1 на тренировку %2").arg(playerId).arg(trainingId));
        refreshTrainings();
        statusBar()->showMessage("Участник записан на тренировку.", 4000);
    } else {
        message("Не удалось записать участника (проверьте ID).");
    }
}

void MainWindow::withdrawTrainingPlayer()
{
    const int trainingId = selectedId(_trainingsTab);
    if (trainingId < 0) {
        message("Выберите тренировку в таблице.");
        return;
    }

    bool ok = false;
    const int playerId = QInputDialog::getInt(
        this, "Снятие с тренировки", "ID участника:", 1, 1, 99999, 1, &ok);
    if (!ok) {
        return;
    }

    if (_club->withdrawPlayer(trainingId, playerId)) {
        logAction(QString("снятие участника %1 с тренировки %2").arg(playerId).arg(trainingId));
        refreshTrainings();
        statusBar()->showMessage("Участник снят с тренировки.", 4000);
    } else {
        message("Не удалось снять участника.");
    }
}

void MainWindow::showTrainingDetails()
{
    const int trainingId = selectedId(_trainingsTab);
    if (trainingId < 0) {
        message("Выберите тренировку в таблице.");
        return;
    }

    std::shared_ptr<TrainingSession> training;
    for (const auto &event : _club->getEvents()) {
        auto candidate = std::dynamic_pointer_cast<TrainingSession>(event);
        if (candidate && candidate->getId() == trainingId) {
            training = candidate;
            break;
        }
    }

    if (!training) {
        message("Тренировка не найдена.");
        return;
    }

    const QString coachName = training->getCoach()
        ? QString::fromStdString(training->getCoach()->getFullName())
        : QString("не назначен");

    QString text = QString("Тренировка: %1 (ID %2)\n"
                           "Тема: %3\n"
                           "Тренер: %4\n"
                           "Длительность: %5 мин\n"
                           "Участники (%6):\n")
                       .arg(QString::fromStdString(std::string(training->getTitle())))
                       .arg(trainingId)
                       .arg(QString::fromStdString(std::string(training->getTopic())))
                       .arg(coachName)
                       .arg(training->getDurationMinutes())
                       .arg(training->getParticipants().size());

    if (training->getParticipants().empty()) {
        text += "Пока никто не записан.";
    } else {
        for (const auto &member : training->getParticipants()) {
            if (member) {
                text += QString("· ID %1: %2\n")
                            .arg(member->getId())
                            .arg(QString::fromStdString(member->getFullName()));
            }
        }
    }

    QMessageBox::information(this, "Детали тренировки", text);
}

void MainWindow::addMatch()
{
    CreateMatchDialog dialog(this);
    for (auto *dateEdit : dialog.findChildren<QDateEdit *>()) {
        dateEdit->setCalendarPopup(true);
        if (auto *cal = dateEdit->calendarWidget()) {
            cal->setMaximumSize(320, 240);
            cal->setGridVisible(true);
        }
    }

    if (dialog.exec() != QDialog::Accepted) {
        return;
    }

    const int id = _club->nextMatchId();
    const DateTime date(dialog.date.day(), dialog.date.month(), dialog.date.year(),
                        dialog.time.hour(), dialog.time.minute(), 0);

    try {
        if (dialog.tournamentId > 0) {
            if (!_club->createMatchInTournament(id, dialog.tournamentId,
                                                dialog.whiteId, dialog.blackId, date)) {
                message("Не удалось: турнир не найден или оба игрока не записаны.");
                return;
            }
        } else {
            auto white = std::dynamic_pointer_cast<Player>(_club->findPlayerById(dialog.whiteId));
            auto black = std::dynamic_pointer_cast<Player>(_club->findPlayerById(dialog.blackId));
            if (!white || !black) {
                message("Игроки не найдены.");
                return;
            }
            _club->createMatch(id, white, black, date);
        }

        logAction(QString("создана партия ID=%1").arg(id));
        refreshAll();
        statusBar()->showMessage(QString("Партия создана, ID %1").arg(id), 4000);
    } catch (const ChessException &e) {
        QMessageBox::warning(this, "Ошибка", e.what());
    }
}

void MainWindow::playMatch()
{
    const int id = selectedId(_matchesTab);
    if (id < 0) {
        message("Выберите партию в таблице.");
        return;
    }

    auto match = _club->findMatchById(id);
    if (!match) {
        message("Партия не найдена.");
        return;
    }
    if (!match->hasGame()) {
        message("У этой партии нет истории ходов (она импортирована из старого формата).");
        return;
    }

    GameDialog dialog(_club.get(), id, this);
    dialog.exec();
    refreshAll();
}

void MainWindow::setMatchResult()
{
    const int id = selectedId(_matchesTab);
    if (id < 0) {
        message("Выберите партию в таблице.");
        return;
    }

    ResultDialog dialog(this);
    if (dialog.exec() != QDialog::Accepted) {
        return;
    }

    if (_club->setMatchResult(id, static_cast<MatchResult::Result>(dialog.result))) {
        logAction(QString("результат партии ID=%1 = %2").arg(id).arg(dialog.result));
        refreshAll();
        statusBar()->showMessage("Результат записан, рейтинги пересчитаны.", 5000);
    } else {
        message("Партия не найдена или результат уже записан.");
    }
}

void MainWindow::deleteMatch()
{
    const int id = selectedId(_matchesTab);
    if (id < 0) {
        message("Выберите партию в таблице.");
        return;
    }

    if (QMessageBox::question(this, "Удаление", QString("Удалить партию ID %1?").arg(id))
        != QMessageBox::Yes) {
        return;
    }

    if (_club->removeMatch(id)) {
        logAction(QString("удалена партия ID=%1").arg(id));
        refreshAll();
        statusBar()->showMessage("Удалено.", 4000);
    } else {
        message("Партия не найдена.");
    }
}

void MainWindow::saveState()
{
    try {
        StorageManager::saveState(filePath(STATE_FILE).toStdString(), *_club);
        logAction("сохранение состояния");
        statusBar()->showMessage("Сохранено в club.txt", 4000);
    } catch (const ChessException &e) {
        QMessageBox::warning(this, "Ошибка сохранения", e.what());
    }
}

void MainWindow::makeReport()
{
    try {
        const QString path = filePath(REPORT_FILE);
        StorageManager::generateReport(path.toStdString(), *_club);
        logAction("сформирован отчёт report.txt");
        statusBar()->showMessage("Отчёт записан в report.txt", 4000);
        openTextFileDialog("Отчёт клуба (report.txt)", path);
    } catch (const ChessException &e) {
        QMessageBox::warning(this, "Ошибка отчёта", e.what());
    }
}

void MainWindow::viewReport()
{
    openTextFileDialog("Отчёт клуба (report.txt)", filePath(REPORT_FILE));
}

void MainWindow::viewLog()
{
    openTextFileDialog("Журнал действий (journal.log)", filePath(LOG_FILE));
}

void MainWindow::applyTheme(const QString &name)
{
    QSettings("chessClub", "chessClubGui").setValue("theme", name);

    QString style;

    if (name == "Тёмная") {
        style = R"(
QWidget { background-color: #1e1e1e; color: #e6e6e6; }
QMainWindow, QDialog, QMessageBox { background-color: #1e1e1e; }
QTableView { background-color: #252526; gridline-color: #3c3c3c; selection-background-color: #0e639c; selection-color: #ffffff; }
QHeaderView::section { background-color: #333337; color: #e6e6e6; border: 1px solid #3c3c3c; padding: 4px; }
QLineEdit, QSpinBox, QComboBox, QDateEdit, QTimeEdit { background-color: #2d2d30; color: #e6e6e6; border: 1px solid #3c3c3c; padding: 3px; }
QPushButton { background-color: #333337; border: 1px solid #3c3c3c; padding: 5px 12px; }
QPushButton:hover { background-color: #3e3e42; }
QPushButton:pressed { background-color: #0e639c; }
QTabWidget::pane { border: 1px solid #3c3c3c; }
QTabBar::tab { background: #2d2d30; color: #e6e6e6; padding: 6px 14px; }
QTabBar::tab:selected { background: #0e639c; }
QMenuBar, QMenu { background-color: #2d2d30; color: #e6e6e6; }
QMenuBar::item:selected, QMenu::item:selected { background: #0e639c; }
QToolTip { background-color: #2d2d30; color: #e6e6e6; border: 1px solid #3c3c3c; padding: 4px; }
QStatusBar { background-color: #2d2d30; color: #cfcfcf; }
)";
    } else if (name == "Синяя") {
        style = R"(
QWidget { background-color: #0f2740; color: #e8f1ff; }
QMainWindow, QDialog, QMessageBox { background-color: #0f2740; }
QTableView { background-color: #143a5e; gridline-color: #1f5178; selection-background-color: #2f7bd0; selection-color: #ffffff; }
QHeaderView::section { background-color: #17466e; color: #e8f1ff; border: 1px solid #1f5178; padding: 4px; }
QLineEdit, QSpinBox, QComboBox, QDateEdit, QTimeEdit { background-color: #143a5e; color: #e8f1ff; border: 1px solid #1f5178; padding: 3px; }
QPushButton { background-color: #17466e; border: 1px solid #1f5178; padding: 5px 12px; }
QPushButton:hover { background-color: #1f5f8f; }
QPushButton:pressed { background-color: #2f7bd0; }
QTabWidget::pane { border: 1px solid #1f5178; }
QTabBar::tab { background: #143a5e; color: #e8f1ff; padding: 6px 14px; }
QTabBar::tab:selected { background: #2f7bd0; }
QMenuBar, QMenu { background-color: #143a5e; color: #e8f1ff; }
QMenuBar::item:selected, QMenu::item:selected { background: #2f7bd0; }
QToolTip { background-color: #143a5e; color: #e8f1ff; border: 1px solid #1f5178; padding: 4px; }
QStatusBar { background-color: #143a5e; color: #cfe2ff; }
)";
    }

    qApp->setStyleSheet(style);

    if (statusBar()) {
        statusBar()->showMessage(QString("Тема: %1").arg(name), 3000);
    }
}