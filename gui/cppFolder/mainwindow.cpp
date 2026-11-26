#include "mainwindow.h"

#include <QAction>
#include <QApplication>
#include <QCalendarWidget>
#include <QComboBox>
#include <QDate>
#include <QDateEdit>
#include <QDialog>
#include <QDialogButtonBox>
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QFormLayout>
#include <QHBoxLayout>
#include <QHeaderView>
#include <QInputDialog>
#include <QLabel>
#include <QLineEdit>
#include <QListWidget>
#include <QMap>
#include <QMenuBar>
#include <QMessageBox>
#include <QPushButton>
#include <QSortFilterProxyModel>
#include <QSpinBox>
#include <QStatusBar>
#include <QTabWidget>
#include <QTableWidget>
#include <QTableWidgetItem>
#include <QTableView>
#include <QTextEdit>
#include <QTimeEdit>
#include <QVBoxLayout>
#include <QVector>
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

QString matchResultLabel(const Match &match)
{
    if (!match.getResult().isPlayed()) {
        return "—";
    }
    const QString raw = QString::fromStdString(match.getResult().toString()).trimmed();
    return raw.isEmpty() ? QString("Завершена") : raw;
}

QString matchStatusLabel(const Match &match)
{
    if (match.getResult().isPlayed()) {
        const QString rawResult = QString::fromStdString(match.getResult().toString()).trimmed();
        return rawResult.isEmpty() ? QString("Сыграна") : QString("Сыграна (%1)").arg(rawResult);
    }
    return "Не сыграна";
}

bool isWeekendDate(const QDate &date)
{
    const int dow = date.dayOfWeek();
    return dow == Qt::Saturday || dow == Qt::Sunday;
}

QStringList allowedSlotsForDate(const QDate &date)
{
    if (isWeekendDate(date)) {
        return {"10:00", "12:00", "14:00", "16:00"};
    }
    return {"17:00", "18:30", "20:00"};
}

constexpr auto STATE_FILE  = "club.txt";
constexpr auto LOG_FILE    = "journal.log";
constexpr auto REPORT_FILE = "report.txt";

QMap<int, Tournament::TimeControl> &standaloneMatchControls()
{
    static QMap<int, Tournament::TimeControl> map;
    return map;
}

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
        2, "Тактика и эндшпиль", "Связанные проходные [25.11.2026 17:00 · будни]", 90, c1);
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
    resize(1120, 680);

    buildUi();
    applyBlueTheme();
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
    _finishedTournaments.clear();
    _completedTrainings.clear();
    _cancelledTrainings.clear();
    standaloneMatchControls().clear();

    if (mode == 1) {
        try {
            auto loaded = std::make_unique<ChessClub>(std::string(_club->getName()));
            StorageManager::loadState(filePath(STATE_FILE).toStdString(), *loaded);
            _club = std::move(loaded);
            populateTournamentParticipantsFromMatches();
            autoPersist();
        } catch (const ChessException &e) {
            QMessageBox::warning(this, "Загрузка", e.what());
        }
    } else if (mode == 2) {
        createTestClub(*_club);
        autoPersist();
    }

    refreshAll();
}

void MainWindow::populateTournamentParticipantsFromMatches()
{
    if (!_club) {
        return;
    }

    for (const auto &match : _club->getMatches()) {
        if (!match || match->getTournamentId() <= 0) {
            continue;
        }
        auto tournament = _club->findTournamentById(match->getTournamentId());
        if (!tournament) {
            continue;
        }

        const int idsToAdd[2] = {
            match->getWhitePlayer() ? match->getWhitePlayer()->getId() : -1,
            match->getBlackPlayer() ? match->getBlackPlayer()->getId() : -1
        };

        for (int pid : idsToAdd) {
            if (pid <= 0) {
                continue;
            }
            bool exists = false;
            for (const auto &p : tournament->getParticipants()) {
                if (p && p->getId() == pid) {
                    exists = true;
                    break;
                }
            }
            if (!exists && static_cast<int>(tournament->getParticipants().size()) < tournament->getMaxPlayers()) {
                if (auto member = _club->findPlayerById(pid)) {
                    *tournament += member;
                }
            }
        }
    }
}

void MainWindow::autoPersist()
{
    if (!_club) {
        return;
    }

    try {
        StorageManager::saveState(filePath(STATE_FILE).toStdString(), *_club);
        StorageManager::generateReport(filePath(REPORT_FILE).toStdString(), *_club);
    } catch (const ChessException &e) {
        statusBar()->showMessage(QString("Ошибка автосохранения: %1").arg(e.what()), 5000);
    }
}

bool MainWindow::isTournamentFinished(int tournamentId) const
{
    if (_finishedTournaments.contains(tournamentId)) {
        return true;
    }
    if (!_club) {
        return false;
    }
    auto tournament = _club->findTournamentById(tournamentId);
    if (!tournament) {
        return false;
    }
    int total = 0;
    int played = 0;
    for (const auto &m : tournament->getMatches()) {
        if (!m) continue;
        ++total;
        if (m->getResult().isPlayed()) {
            ++played;
        }
    }
    return total > 0 && played == total;
}

QString MainWindow::tournamentStatusLabel(int tournamentId) const
{
    return isTournamentFinished(tournamentId) ? QString("Завершён") : QString("Идёт");
}

bool MainWindow::isTrainingCompleted(int trainingId) const
{
    return _completedTrainings.contains(trainingId);
}

bool MainWindow::isTrainingCancelled(int trainingId) const
{
    return _cancelledTrainings.contains(trainingId);
}

QString MainWindow::trainingStatusLabel(int trainingId) const
{
    if (_completedTrainings.contains(trainingId)) {
        return "Завершена (+ELO)";
    }
    if (_cancelledTrainings.contains(trainingId)) {
        return "Отменена";
    }
    return "Запланирована";
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

    _tabs = new QTabWidget(this);
    setCentralWidget(_tabs);

    auto attachSideActions = [](QWidget *page, QTableView *view, const QVector<QPushButton *> &buttons) {
        auto *rootLayout = qobject_cast<QVBoxLayout *>(page->layout());
        if (!rootLayout || !view) {
            return;
        }

        rootLayout->removeWidget(view);

        auto *bodyLayout = new QHBoxLayout;
        bodyLayout->setSpacing(14);
        bodyLayout->addWidget(view, 1);

        auto *sidePanel = new QVBoxLayout;
        sidePanel->setSpacing(10);
        for (QPushButton *btn : buttons) {
            btn->setFixedWidth(185);
            btn->setSizePolicy(QSizePolicy::Fixed, QSizePolicy::Fixed);
            sidePanel->addWidget(btn);
        }
        sidePanel->addStretch();

        auto *panelWrapper = new QWidget(page);
        panelWrapper->setLayout(sidePanel);
        panelWrapper->setSizePolicy(QSizePolicy::Fixed, QSizePolicy::Preferred);
        bodyLayout->addWidget(panelWrapper, 0);

        rootLayout->addLayout(bodyLayout, 1);
    };

    {
        auto *page = buildTableTab(_playersTab,
            {"ID", "ФИО", "Классика", "Рапид", "Блиц", "В", "Н", "П"});

        auto *add = new QPushButton("Добавить", page);
        auto *remove = new QPushButton("Удалить", page);
        auto *history = new QPushButton("История матчей", page);
        attachSideActions(page, _playersTab.view, {add, remove, history});

        connect(add, &QPushButton::clicked, this, &MainWindow::addPlayer);
        connect(remove, &QPushButton::clicked, this, &MainWindow::deletePlayer);
        connect(history, &QPushButton::clicked, this, &MainWindow::showPlayerHistory);

        _tabs->addTab(page, "Игроки");
    }

    {
        auto *page = buildTableTab(_coachesTab,
            {"ID", "ФИО", "Классика", "Рапид", "Блиц", "Стаж", "Специализация"});

        auto *add = new QPushButton("Добавить", page);
        auto *remove = new QPushButton("Удалить", page);
        attachSideActions(page, _coachesTab.view, {add, remove});

        connect(add, &QPushButton::clicked, this, &MainWindow::addCoach);
        connect(remove, &QPushButton::clicked, this, &MainWindow::deleteCoach);

        _tabs->addTab(page, "Тренеры");
    }

    {
        auto *page = buildTableTab(_tournamentsTab,
            {"ID", "Название", "Контроль", "Система", "Участники", "Лимит", "Партии", "Статус"});

        auto *add = new QPushButton("Добавить", page);
        auto *info = new QPushButton("Инфо", page);
        auto *finish = new QPushButton("Завершить", page);
        auto *remove = new QPushButton("Удалить", page);
        attachSideActions(page, _tournamentsTab.view, {add, info, finish, remove});

        connect(add, &QPushButton::clicked, this, &MainWindow::addTournament);
        connect(info, &QPushButton::clicked, this, &MainWindow::showTournamentInfo);
        connect(finish, &QPushButton::clicked, this, &MainWindow::finishTournament);
        connect(remove, &QPushButton::clicked, this, &MainWindow::deleteTournament);
        connect(_tournamentsTab.view, &QTableView::doubleClicked, this,
                [this](const QModelIndex &) { showTournamentInfo(); });

        _tabs->addTab(page, "Турниры");
    }

    {
        auto *page = buildTableTab(_trainingsTab,
            {"ID", "Название", "Тема и время", "Тренер", "Длительность (мин)", "Участники", "Статус"});

        auto *add = new QPushButton("Добавить", page);
        auto *info = new QPushButton("Инфо", page);
        auto *finish = new QPushButton("Завершить (+ELO)", page);
        auto *cancel = new QPushButton("Отменить", page);
        auto *remove = new QPushButton("Удалить", page);
        attachSideActions(page, _trainingsTab.view, {add, info, finish, cancel, remove});

        connect(add, &QPushButton::clicked, this, &MainWindow::addTraining);
        connect(info, &QPushButton::clicked, this, &MainWindow::showTrainingDetails);
        connect(finish, &QPushButton::clicked, this, &MainWindow::finishTraining);
        connect(cancel, &QPushButton::clicked, this, &MainWindow::cancelTraining);
        connect(remove, &QPushButton::clicked, this, &MainWindow::deleteTraining);
        connect(_trainingsTab.view, &QTableView::doubleClicked, this,
                [this](const QModelIndex &) { showTrainingDetails(); });

        _tabs->addTab(page, "Тренировки");
    }

    {
        auto *page = buildTableTab(_matchesTab,
            {"ID", "Белые", "Чёрные", "Дата", "Статус", "Турнир"}, true);

        auto *play = new QPushButton("Играть", page);
        auto *add = new QPushButton("Создать", page);
        auto *result = new QPushButton("Результат", page);
        auto *remove = new QPushButton("Удалить", page);
        attachSideActions(page, _matchesTab.view, {play, add, result, remove});

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
        filterRow->setSpacing(10);

        tab.statusFilter = new QComboBox(page);
        tab.statusFilter->addItem("Все матчи", 0);
        tab.statusFilter->addItem("Несыгранные", 1);
        tab.statusFilter->addItem("Сыгранные", 2);
        tab.statusFilter->setMinimumWidth(160);

        auto *tournamentFilter = new QComboBox(page);
        tournamentFilter->setObjectName("matchTournamentFilter");
        tournamentFilter->addItem("Все турниры и вне турнира", -999);
        tournamentFilter->addItem("Только вне турнира", 0);
        tournamentFilter->setMinimumWidth(220);

        filterRow->addWidget(tab.statusFilter);
        filterRow->addWidget(tournamentFilter);
        filterRow->addStretch();
        layout->addLayout(filterRow);

        connect(tab.statusFilter, &QComboBox::currentIndexChanged, this, [this]() { refreshMatches(); });
        connect(tournamentFilter, &QComboBox::currentIndexChanged, this, [this]() { refreshMatches(); });
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
            QString::fromStdString(coach->getSpecialization().toString())
        };
        row.values = {
            coach->getId(),
            QString::fromStdString(coach->getFullName()),
            coach->getRatings().getClassical(),
            coach->getRatings().getRapid(),
            coach->getRatings().getBlitz(),
            coach->getExperienceYears(),
            QString::fromStdString(coach->getSpecialization().toString())
        };
        rows.push_back(row);
    }

    _coachesTab.model->resetRows(
        {"ID", "ФИО", "Классика", "Рапид", "Блиц", "Стаж", "Специализация"}, rows);
}

void MainWindow::refreshTournaments()
{
    QVector<ChessTableModel::Row> rows;

    for (const auto &event : _club->getEvents()) {
        auto tournament = std::dynamic_pointer_cast<Tournament>(event);
        if (!tournament) {
            continue;
        }

        const int tid = tournament->getId();
        const QString statusStr = tournamentStatusLabel(tid);

        ChessTableModel::Row row;
        row.id = tid;
        row.muted = isTournamentFinished(tid);
        row.text = {
            QString::number(tid),
            QString::fromStdString(std::string(tournament->getTitle())),
            timeControlName(tournament->getTimeControl()),
            QString::fromStdString(tournament->getSystem()->getName()),
            QString::number(tournament->getParticipants().size()),
            QString::number(tournament->getMaxPlayers()),
            QString::number(tournament->getMatches().size()),
            statusStr
        };
        row.values = {
            tid,
            QString::fromStdString(std::string(tournament->getTitle())),
            timeControlName(tournament->getTimeControl()),
            QString::fromStdString(tournament->getSystem()->getName()),
            static_cast<int>(tournament->getParticipants().size()),
            tournament->getMaxPlayers(),
            static_cast<int>(tournament->getMatches().size()),
            statusStr
        };
        rows.push_back(row);
    }

    _tournamentsTab.model->resetRows(
        {"ID", "Название", "Контроль", "Система", "Участники", "Лимит", "Партии", "Статус"}, rows);
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
        const QString statusStr = trainingStatusLabel(training->getId());

        ChessTableModel::Row row;
        row.id = training->getId();
        row.muted = isTrainingCancelled(training->getId());
        row.text = {
            QString::number(training->getId()),
            QString::fromStdString(std::string(training->getTitle())),
            QString::fromStdString(std::string(training->getTopic())),
            coachName,
            QString::number(training->getDurationMinutes()),
            QString::number(training->getParticipants().size()),
            statusStr
        };
        row.values = {
            training->getId(),
            QString::fromStdString(std::string(training->getTitle())),
            QString::fromStdString(std::string(training->getTopic())),
            coachName,
            training->getDurationMinutes(),
            static_cast<int>(training->getParticipants().size()),
            statusStr
        };
        rows.push_back(row);
    }

    _trainingsTab.model->resetRows(
        {"ID", "Название", "Тема и время", "Тренер", "Длительность (мин)", "Участники", "Статус"}, rows);
}

void MainWindow::refreshMatches()
{
    auto *tournamentFilter = _matchesTab.view
        ? _matchesTab.view->parentWidget()->findChild<QComboBox *>("matchTournamentFilter")
        : nullptr;

    if (tournamentFilter) {
        const int prevVal = tournamentFilter->currentData().toInt();
        const QSignalBlocker blocker(tournamentFilter);
        tournamentFilter->clear();
        tournamentFilter->addItem("Все матчи", -999);
        tournamentFilter->addItem("Вне турнира", 0);
        for (const auto &event : _club->getEvents()) {
            auto tournament = std::dynamic_pointer_cast<Tournament>(event);
            if (tournament) {
                tournamentFilter->addItem(
                    QString("Турнир #%1 · %2")
                        .arg(tournament->getId())
                        .arg(QString::fromStdString(std::string(tournament->getTitle()))),
                    tournament->getId());
            }
        }
        const int foundIdx = tournamentFilter->findData(prevVal);
        tournamentFilter->setCurrentIndex(foundIdx >= 0 ? foundIdx : 0);
    }

    const int filterMode = _matchesTab.statusFilter ? _matchesTab.statusFilter->currentData().toInt() : 0;
    const int tournamentFilterId = tournamentFilter ? tournamentFilter->currentData().toInt() : -999;

    QVector<ChessTableModel::Row> rows;
    for (const auto &match : _club->getMatches()) {
        if (!match) {
            continue;
        }

        const int tid = match->getTournamentId();
        if (tournamentFilterId == 0 && tid > 0) {
            continue;
        }
        if (tournamentFilterId > 0 && tid != tournamentFilterId) {
            continue;
        }

        const bool played = match->getResult().isPlayed();
        if (filterMode == 1 && played) {
            continue;
        }
        if (filterMode == 2 && !played) {
            continue;
        }

        const QString statusStr = matchStatusLabel(*match);
        const QString tournamentStr = tid > 0 ? QString::number(tid) : QString("—");

        ChessTableModel::Row row;
        row.id = match->getId();
        row.text = {
            QString::number(match->getId()),
            QString::fromStdString(match->getWhitePlayer()->getFullName()),
            QString::fromStdString(match->getBlackPlayer()->getFullName()),
            QString::fromStdString(match->getDate().toString()),
            statusStr,
            tournamentStr
        };
        row.values = {
            match->getId(),
            QString::fromStdString(match->getWhitePlayer()->getFullName()),
            QString::fromStdString(match->getBlackPlayer()->getFullName()),
            QString::fromStdString(match->getDate().toString()),
            statusStr,
            tid
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
        autoPersist();
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
        autoPersist();
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
        autoPersist();
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
        autoPersist();
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
        autoPersist();
        refreshAll();
        statusBar()->showMessage(QString("Турнир добавлен, ID %1").arg(id), 4000);
    } catch (const ChessException &e) {
        QMessageBox::warning(this, "Ошибка", e.what());
    }
}

void MainWindow::finishTournament()
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

    if (_finishedTournaments.contains(tournamentId)) {
        message("Турнир уже имеет статус «Завершён» и защищён от изменений.");
        return;
    }

    if (QMessageBox::question(
            this,
            "Завершить турнир",
            QString("Завершить турнир «%1» (ID %2)?\n"
                    "После завершения нельзя будет менять состав участников или удалить турнир.")
                .arg(QString::fromStdString(std::string(tournament->getTitle())))
                .arg(tournamentId))
        != QMessageBox::Yes) {
        return;
    }

    _finishedTournaments.insert(tournamentId);
    logAction(QString("завершён турнир ID=%1").arg(tournamentId));
    autoPersist();
    refreshTournaments();
    statusBar()->showMessage(QString("Турнир ID %1 завершён.").arg(tournamentId), 4000);
}

void MainWindow::deleteTournament()
{
    const int id = selectedId(_tournamentsTab);
    if (id < 0) {
        message("Выберите турнир в таблице.");
        return;
    }

    if (isTournamentFinished(id)) {
        message("Турнир завершён и защищён от изменений — удалить его нельзя.");
        return;
    }

    if (QMessageBox::question(this, "Удаление",
                              QString("Удалить турнир ID %1 вместе с его партиями?").arg(id))
        != QMessageBox::Yes) {
        return;
    }

    if (_club->removeEvent(id)) {
        _finishedTournaments.remove(id);
        logAction(QString("удалён турнир ID=%1").arg(id));
        autoPersist();
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

    if (isTournamentFinished(tournamentId)) {
        message("Турнир завершён — изменять состав участников нельзя.");
        return;
    }

    auto tournament = _club->findTournamentById(tournamentId);
    if (!tournament) {
        message("Турнир не найден.");
        return;
    }

    QDialog dialog(this);
    dialog.setWindowTitle("Запись игроков на турнир");
    dialog.resize(460, 360);

    auto *layout = new QVBoxLayout(&dialog);
    auto *headerLabel = new QLabel(&dialog);
    layout->addWidget(headerLabel);

    auto *currentList = new QListWidget(&dialog);
    layout->addWidget(new QLabel("Уже записаны:", &dialog));
    layout->addWidget(currentList, 1);

    auto *combo = new QComboBox(&dialog);
    layout->addWidget(new QLabel("Выберите игрока для добавления:", &dialog));

    auto *addRow = new QHBoxLayout;
    auto *enrollBtn = new QPushButton("Записать", &dialog);
    addRow->addWidget(combo, 1);
    addRow->addWidget(enrollBtn);
    layout->addLayout(addRow);

    auto *closeBtn = new QPushButton("Закрыть", &dialog);
    auto *bottomRow = new QHBoxLayout;
    bottomRow->addStretch();
    bottomRow->addWidget(closeBtn);
    layout->addLayout(bottomRow);

    auto rebuildLists = [&]() {
        headerLabel->setText(QString("Турнир: %1 (записано %2/%3)")
                                 .arg(QString::fromStdString(std::string(tournament->getTitle())))
                                 .arg(tournament->getParticipants().size())
                                 .arg(tournament->getMaxPlayers()));

        currentList->clear();
        for (const auto &member : tournament->getParticipants()) {
            if (member) {
                currentList->addItem(QString("ID %1 · %2")
                                         .arg(member->getId())
                                         .arg(QString::fromStdString(member->getFullName())));
            }
        }

        combo->clear();
        for (const auto &member : _club->getPlayers()) {
            auto player = std::dynamic_pointer_cast<Player>(member);
            if (!player) {
                continue;
            }
            bool already = false;
            for (const auto &p : tournament->getParticipants()) {
                if (p && p->getId() == player->getId()) {
                    already = true;
                    break;
                }
            }
            if (!already) {
                combo->addItem(QString("ID %1 · %2 (рейтинг %3)")
                                   .arg(player->getId())
                                   .arg(QString::fromStdString(player->getFullName()))
                                   .arg(player->getRatings().getClassical()),
                               player->getId());
            }
        }

        const bool locked = isTournamentFinished(tournamentId);
        const bool hasSpace = static_cast<int>(tournament->getParticipants().size()) < tournament->getMaxPlayers();
        enrollBtn->setEnabled(!locked && combo->count() > 0 && hasSpace);
    };

    rebuildLists();

    connect(enrollBtn, &QPushButton::clicked, &dialog, [&]() {
        if (isTournamentFinished(tournamentId)) {
            QMessageBox::information(&dialog, "Турнир завершён",
                                         "Турнир завершён — изменять состав участников нельзя.");
            return;
        }
        if (combo->count() == 0) {
            return;
        }
        const int playerId = combo->currentData().toInt();
        if (_club->enrollPlayer(tournamentId, playerId)) {
            logAction(QString("запись игрока %1 на турнир %2").arg(playerId).arg(tournamentId));
            autoPersist();
            refreshTournaments();
            rebuildLists();
            statusBar()->showMessage(QString("Игрок ID %1 записан.").arg(playerId), 4000);
        } else {
            QMessageBox::warning(&dialog, "Запись на турнир",
                                 "Не удалось: достигнут лимит участников или игрок уже записан.");
        }
    });

    connect(closeBtn, &QPushButton::clicked, &dialog, &QDialog::accept);
    dialog.exec();
}

void MainWindow::withdrawPlayer()
{
    const int tournamentId = selectedId(_tournamentsTab);
    if (tournamentId < 0) {
        message("Выберите турнир в таблице.");
        return;
    }

    if (isTournamentFinished(tournamentId)) {
        message("Турнир завершён — снимать игроков нельзя.");
        return;
    }

    auto tournament = _club->findTournamentById(tournamentId);
    if (!tournament) {
        message("Турнир не найден.");
        return;
    }

    if (tournament->getParticipants().empty()) {
        message("В этом турнире пока нет записанных участников.");
        return;
    }

    QDialog dialog(this);
    dialog.setWindowTitle("Снятие игрока с турнира");
    dialog.resize(420, 280);

    auto *layout = new QVBoxLayout(&dialog);
    layout->addWidget(new QLabel(QString("Турнир: %1")
                                     .arg(QString::fromStdString(std::string(tournament->getTitle()))),
                                 &dialog));
    layout->addWidget(new QLabel("Выберите участника для снятия:", &dialog));

    auto *listWidget = new QListWidget(&dialog);
    for (const auto &member : tournament->getParticipants()) {
        if (member) {
            auto *item = new QListWidgetItem(QString("ID %1 · %2")
                                                .arg(member->getId())
                                                .arg(QString::fromStdString(member->getFullName())),
                                            listWidget);
            item->setData(Qt::UserRole, member->getId());
        }
    }
    listWidget->setCurrentRow(0);
    layout->addWidget(listWidget, 1);

    auto *buttons = new QDialogButtonBox(QDialogButtonBox::Ok | QDialogButtonBox::Cancel, &dialog);
    layout->addWidget(buttons);
    connect(buttons, &QDialogButtonBox::accepted, &dialog, &QDialog::accept);
    connect(buttons, &QDialogButtonBox::rejected, &dialog, &QDialog::reject);

    if (dialog.exec() != QDialog::Accepted || !listWidget->currentItem()) {
        return;
    }

    const int playerId = listWidget->currentItem()->data(Qt::UserRole).toInt();
    if (_club->withdrawPlayer(tournamentId, playerId)) {
        logAction(QString("снятие игрока %1 с турнира %2").arg(playerId).arg(tournamentId));
        autoPersist();
        refreshAll();
        statusBar()->showMessage("Игрок снят.", 4000);
    } else {
        message("Не удалось: игрок не был записан.");
    }
}

void MainWindow::showTournamentInfo()
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

    auto *dialog = new QDialog(this);
    dialog->setWindowTitle(QString("Турнир: %1")
                               .arg(QString::fromStdString(std::string(tournament->getTitle()))));
    dialog->resize(820, 560);

    auto *mainLayout = new QVBoxLayout(dialog);
    auto *summaryLabel = new QLabel(dialog);
    mainLayout->addWidget(summaryLabel);

    auto *subTabs = new QTabWidget(dialog);
    mainLayout->addWidget(subTabs, 1);

    auto *resultsPage = new QWidget(subTabs);
    auto *resultsLayout = new QVBoxLayout(resultsPage);
    auto *standingsTable = new QTableWidget(resultsPage);
    standingsTable->setColumnCount(5);
    standingsTable->setHorizontalHeaderLabels({"Место", "Шахматист", "Рейтинг", "Очки", "В / Н / П"});
    standingsTable->setEditTriggers(QAbstractItemView::NoEditTriggers);
    standingsTable->setSelectionBehavior(QAbstractItemView::SelectRows);
    standingsTable->setSelectionMode(QAbstractItemView::SingleSelection);
    standingsTable->verticalHeader()->setVisible(false);
    standingsTable->horizontalHeader()->setStretchLastSection(true);
    resultsLayout->addWidget(standingsTable);
    subTabs->addTab(resultsPage, "Результаты");

    auto *matchesPage = new QWidget(subTabs);
    auto *matchesLayout = new QVBoxLayout(matchesPage);
    auto *matchesTable = new QTableWidget(matchesPage);
    matchesTable->setColumnCount(6);
    matchesTable->setHorizontalHeaderLabels({"ID", "Дата", "Белые", "Чёрные", "Результат", "Статус"});
    matchesTable->setEditTriggers(QAbstractItemView::NoEditTriggers);
    matchesTable->setSelectionBehavior(QAbstractItemView::SelectRows);
    matchesTable->setSelectionMode(QAbstractItemView::SingleSelection);
    matchesTable->verticalHeader()->setVisible(false);
    matchesTable->horizontalHeader()->setStretchLastSection(true);
    matchesLayout->addWidget(matchesTable, 1);

    auto *matchesBtnRow = new QHBoxLayout;
    auto *addMatchBtn = new QPushButton("Добавить партию", matchesPage);
    auto *playMatchBtn = new QPushButton("Играть", matchesPage);
    auto *setResultBtn = new QPushButton("Результат", matchesPage);
    matchesBtnRow->addWidget(addMatchBtn);
    matchesBtnRow->addWidget(playMatchBtn);
    matchesBtnRow->addWidget(setResultBtn);
    matchesBtnRow->addStretch();
    matchesLayout->addLayout(matchesBtnRow);
    subTabs->addTab(matchesPage, "Партии");

    auto *playersPage = new QWidget(subTabs);
    auto *playersLayout = new QVBoxLayout(playersPage);
    auto *playersTable = new QTableWidget(playersPage);
    playersTable->setColumnCount(3);
    playersTable->setHorizontalHeaderLabels({"ID", "ФИО", "Рейтинг (классика)"});
    playersTable->setEditTriggers(QAbstractItemView::NoEditTriggers);
    playersTable->setSelectionBehavior(QAbstractItemView::SelectRows);
    playersTable->setSelectionMode(QAbstractItemView::SingleSelection);
    playersTable->verticalHeader()->setVisible(false);
    playersTable->horizontalHeader()->setStretchLastSection(true);
    playersLayout->addWidget(playersTable, 1);

    auto *playersBtnRow = new QHBoxLayout;
    auto *addPlayerBtn = new QPushButton("Записать...", playersPage);
    auto *removePlayerBtn = new QPushButton("Снять выбранного", playersPage);
    playersBtnRow->addWidget(addPlayerBtn);
    playersBtnRow->addWidget(removePlayerBtn);
    playersBtnRow->addStretch();
    playersLayout->addLayout(playersBtnRow);
    subTabs->addTab(playersPage, "Шахматисты");

    auto reloadTournamentView = [&]() {
        const auto &tMatches = tournament->getMatches();
        int totalMatches = 0;
        int playedCount = 0;
        for (const auto &m : tMatches) {
            if (!m) continue;
            ++totalMatches;
            if (m->getResult().isPlayed()) {
                ++playedCount;
            }
        }

        const bool finished = isTournamentFinished(tournamentId);
        const QString phaseText = finished
            ? QString("Завершён")
            : QString("Идёт (%1/%2 партий сыграно)").arg(playedCount).arg(totalMatches);

        int enrolledPlayersCount = 0;
        for (const auto &member : tournament->getParticipants()) {
            if (member) {
                ++enrolledPlayersCount;
            }
        }

        addPlayerBtn->setEnabled(!finished);
        removePlayerBtn->setEnabled(!finished);
        addMatchBtn->setEnabled(!finished && enrolledPlayersCount >= 2);
        playMatchBtn->setEnabled(!finished && totalMatches > 0);
        setResultBtn->setEnabled(!finished && totalMatches > 0);

        summaryLabel->setText(
            QString("<b>%1</b> (ID %2) · Контроль: %3 · Система: %4 · Участники: %5/%6 · Статус: <b>%7</b>")
                .arg(QString::fromStdString(std::string(tournament->getTitle())))
                .arg(tournament->getId())
                .arg(timeControlName(tournament->getTimeControl()))
                .arg(QString::fromStdString(tournament->getSystem()->getName()))
                .arg(tournament->getParticipants().size())
                .arg(tournament->getMaxPlayers())
                .arg(phaseText));

        const auto standings = tournament->getStandings(true);
        standingsTable->setRowCount(static_cast<int>(standings.size()));
        for (int i = 0; i < static_cast<int>(standings.size()); ++i) {
            const auto &entry = standings[static_cast<size_t>(i)];
            const int rating = entry.player ? entry.player->getRatings().getClassical() : 0;
            const QString name = entry.player
                ? QString::fromStdString(entry.player->getFullName())
                : QString("—");

            auto *posItem = new QTableWidgetItem(QString("#%1").arg(i + 1));
            posItem->setData(Qt::UserRole, entry.player ? entry.player->getId() : -1);

            standingsTable->setItem(i, 0, posItem);
            standingsTable->setItem(i, 1, new QTableWidgetItem(name));
            standingsTable->setItem(i, 2, new QTableWidgetItem(QString::number(rating)));
            standingsTable->setItem(i, 3, new QTableWidgetItem(QString::number(entry.points)));
            standingsTable->setItem(i, 4, new QTableWidgetItem(
                                              QString("%1 / %2 / %3")
                                                  .arg(entry.wins)
                                                  .arg(entry.draws)
                                                  .arg(entry.losses)));
        }
        standingsTable->resizeColumnsToContents();

        matchesTable->setRowCount(totalMatches);
        int matchRow = 0;
        for (const auto &m : tMatches) {
            if (!m) continue;

            const QString whiteName = m->getWhitePlayer()
                ? QString::fromStdString(m->getWhitePlayer()->getFullName())
                : QString("—");
            const QString blackName = m->getBlackPlayer()
                ? QString::fromStdString(m->getBlackPlayer()->getFullName())
                : QString("—");
            const QString resStr = matchResultLabel(*m);
            const QString stStr = m->getResult().isPlayed() ? QString("Сыграна") : QString("Не сыграна");

            auto *idItem = new QTableWidgetItem(QString::number(m->getId()));
            idItem->setData(Qt::UserRole, m->getId());

            matchesTable->setItem(matchRow, 0, idItem);
            matchesTable->setItem(matchRow, 1, new QTableWidgetItem(QString::fromStdString(m->getDate().toString())));
            matchesTable->setItem(matchRow, 2, new QTableWidgetItem(whiteName));
            matchesTable->setItem(matchRow, 3, new QTableWidgetItem(blackName));
            matchesTable->setItem(matchRow, 4, new QTableWidgetItem(resStr));
            matchesTable->setItem(matchRow, 5, new QTableWidgetItem(stStr));
            ++matchRow;
        }
        if (totalMatches > 0 && matchesTable->currentRow() < 0) {
            matchesTable->setCurrentCell(0, 0);
        }
        matchesTable->resizeColumnsToContents();

        playersTable->setRowCount(enrolledPlayersCount);

        int playerRow = 0;
        for (const auto &member : tournament->getParticipants()) {
            if (!member) continue;

            auto player = std::dynamic_pointer_cast<Player>(member);
            const int rating = player ? player->getRatings().getClassical() : member->getRatings().getClassical();

            auto *idItem = new QTableWidgetItem(QString::number(member->getId()));
            idItem->setData(Qt::UserRole, member->getId());

            playersTable->setItem(playerRow, 0, idItem);
            playersTable->setItem(playerRow, 1, new QTableWidgetItem(QString::fromStdString(member->getFullName())));
            playersTable->setItem(playerRow, 2, new QTableWidgetItem(QString::number(rating)));
            ++playerRow;
        }
        playersTable->resizeColumnsToContents();
    };

    reloadTournamentView();

    connect(addMatchBtn, &QPushButton::clicked, dialog, [&]() {
        if (isTournamentFinished(tournamentId)) {
            QMessageBox::information(dialog, "Турнир завершён",
                                     "Турнир завершён — добавлять новые партии нельзя.");
            return;
        }

        QVector<std::shared_ptr<Player>> eligiblePlayers;
        for (const auto &member : tournament->getParticipants()) {
            auto player = std::dynamic_pointer_cast<Player>(member);
            if (player) {
                eligiblePlayers.push_back(player);
            }
        }

        if (eligiblePlayers.size() < 2) {
            QMessageBox::information(dialog, "Недостаточно игроков",
                                     "Для создания партии в турнире должно быть записано минимум 2 игрока.");
            return;
        }

        QDialog createDlg(dialog);
        createDlg.setWindowTitle("Новая партия в турнире");
        createDlg.resize(420, 250);

        auto *dlgLayout = new QVBoxLayout(&createDlg);
        auto *form = new QFormLayout;

        auto *whiteCombo = new QComboBox(&createDlg);
        auto *blackCombo = new QComboBox(&createDlg);
        for (const auto &p : eligiblePlayers) {
            const QString label = QString("ID %1 · %2").arg(p->getId()).arg(QString::fromStdString(p->getFullName()));
            whiteCombo->addItem(label, p->getId());
            blackCombo->addItem(label, p->getId());
        }
        if (blackCombo->count() > 1) {
            blackCombo->setCurrentIndex(1);
        }

        auto *dateEdit = new QDateEdit(QDate::currentDate(), &createDlg);
        dateEdit->setCalendarPopup(true);
        dateEdit->setDisplayFormat("dd.MM.yyyy");

        auto *timeEdit = new QTimeEdit(QTime(15, 0), &createDlg);
        timeEdit->setDisplayFormat("HH:mm");

        form->addRow("Белые:", whiteCombo);
        form->addRow("Чёрные:", blackCombo);
        form->addRow("Дата:", dateEdit);
        form->addRow("Время:", timeEdit);
        dlgLayout->addLayout(form);

        auto *btnBox = new QDialogButtonBox(QDialogButtonBox::Ok | QDialogButtonBox::Cancel, &createDlg);
        dlgLayout->addWidget(btnBox);
        connect(btnBox, &QDialogButtonBox::accepted, &createDlg, [&]() {
            const int wId = whiteCombo->currentData().toInt();
            const int bId = blackCombo->currentData().toInt();
            if (wId == bId) {
                QMessageBox::warning(&createDlg, "Проверка игроков",
                                     "Игрок не может сыграть партию сам с собой. Выберите разных игроков.");
                return;
            }
            createDlg.accept();
        });
        connect(btnBox, &QDialogButtonBox::rejected, &createDlg, &QDialog::reject);

        if (createDlg.exec() != QDialog::Accepted) {
            return;
        }

        const int whiteId = whiteCombo->currentData().toInt();
        const int blackId = blackCombo->currentData().toInt();
        const int newMatchId = _club->nextMatchId();
        const DateTime dt(dateEdit->date().day(), dateEdit->date().month(), dateEdit->date().year(),
                          timeEdit->time().hour(), timeEdit->time().minute(), 0);

        try {
            if (!_club->createMatchInTournament(newMatchId, tournamentId, whiteId, blackId, dt)) {
                QMessageBox::warning(dialog, "Ошибка",
                                     "Не удалось создать партию в турнире.");
                return;
            }
            if (auto created = _club->findMatchById(newMatchId)) {
                created->setTournamentId(tournamentId);
            }
            logAction(QString("создана партия ID=%1 в турнире ID=%2").arg(newMatchId).arg(tournamentId));
            autoPersist();
            refreshAll();
            reloadTournamentView();
            statusBar()->showMessage(QString("Партия ID %1 добавлена в турнир.").arg(newMatchId), 4000);
        } catch (const ChessException &e) {
            QMessageBox::warning(dialog, "Ошибка", e.what());
        }
    });

    connect(playMatchBtn, &QPushButton::clicked, dialog, [&]() {
        const int row = matchesTable->currentRow();
        if (row < 0 || !matchesTable->item(row, 0)) {
            QMessageBox::information(dialog, "Партия", "Выберите партию в списке.");
            return;
        }
        const int matchId = matchesTable->item(row, 0)->data(Qt::UserRole).toInt();
        auto match = _club->findMatchById(matchId);
        if (!match) {
            QMessageBox::information(dialog, "Партия", "Партия не найдена.");
            return;
        }
        if (match->getResult().isPlayed()) {
            QMessageBox::information(dialog, "Партия", "Этот матч уже сыгран — сыграть его заново нельзя.");
            return;
        }
        if (match->getWhitePlayer() && match->getBlackPlayer()
            && match->getWhitePlayer()->getId() == match->getBlackPlayer()->getId()) {
            QMessageBox::warning(dialog, "Партия", "Некорректная партия: игрок не может играть сам с собой.");
            return;
        }
        if (!match->hasGame()) {
            QMessageBox::information(dialog, "Партия",
                                     "У этой партии нет истории ходов (она импортирована из старого формата).");
            return;
        }

        GameDialog gameDlg(_club.get(), matchId, tournament->getTimeControl(), dialog);
        gameDlg.exec();
        autoPersist();
        refreshAll();
        reloadTournamentView();
    });

    connect(setResultBtn, &QPushButton::clicked, dialog, [&]() {
        const int row = matchesTable->currentRow();
        if (row < 0 || !matchesTable->item(row, 0)) {
            QMessageBox::information(dialog, "Результат", "Выберите партию в списке.");
            return;
        }
        const int matchId = matchesTable->item(row, 0)->data(Qt::UserRole).toInt();
        ResultDialog resDlg(dialog);
        if (resDlg.exec() != QDialog::Accepted) {
            return;
        }
        if (_club->setMatchResult(matchId, static_cast<MatchResult::Result>(resDlg.result))) {
            logAction(QString("результат партии ID=%1 в турнире ID=%2 = %3")
                          .arg(matchId).arg(tournamentId).arg(resDlg.result));
            autoPersist();
            refreshAll();
            reloadTournamentView();
            statusBar()->showMessage("Результат записан, рейтинги пересчитаны.", 5000);
        } else {
            QMessageBox::information(dialog, "Результат",
                                     "Партия не найдена или результат уже записан.");
        }
    });

    connect(addPlayerBtn, &QPushButton::clicked, dialog, [&]() {
        if (isTournamentFinished(tournamentId)) {
            QMessageBox::information(dialog, "Турнир завершён",
                                     "Турнир завершён — изменять состав участников нельзя.");
            return;
        }
        _tournamentsTab.view->setFocus();
        enrollPlayer();
        reloadTournamentView();
    });

    connect(removePlayerBtn, &QPushButton::clicked, dialog, [&]() {
        if (isTournamentFinished(tournamentId)) {
            QMessageBox::information(dialog, "Турнир завершён",
                                     "Турнир завершён — снимать игроков нельзя.");
            return;
        }
        const int row = playersTable->currentRow();
        if (row < 0 || !playersTable->item(row, 0)) {
            message("Выберите шахматиста в списке.");
            return;
        }
        const int pid = playersTable->item(row, 0)->data(Qt::UserRole).toInt();
        if (_club->withdrawPlayer(tournamentId, pid)) {
            logAction(QString("снятие игрока %1 с турнира %2").arg(pid).arg(tournamentId));
            autoPersist();
            refreshAll();
            reloadTournamentView();
        }
    });

    auto *closeRow = new QHBoxLayout;
    auto *closeBtn = new QPushButton("Закрыть", dialog);
    closeRow->addStretch();
    closeRow->addWidget(closeBtn);
    mainLayout->addLayout(closeRow);
    connect(closeBtn, &QPushButton::clicked, dialog, &QDialog::accept);

    dialog->exec();
    dialog->deleteLater();
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
        text += QString("Победитель / лидер: %1 (очки: %2)\n\n")
                    .arg(QString::fromStdString(standings.front().player->getFullName()))
                    .arg(standings.front().points);

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
    QDialog dialog(this);
    dialog.setWindowTitle("Новая тренировка");
    dialog.resize(440, 310);

    auto *layout = new QVBoxLayout(&dialog);
    auto *form = new QFormLayout;

    auto *titleEdit = new QLineEdit(&dialog);
    titleEdit->setPlaceholderText("Например: Эндшпиль и расчёт");

    auto *topicEdit = new QLineEdit("Общая практика", &dialog);

    auto *dateEdit = new QDateEdit(QDate::currentDate(), &dialog);
    dateEdit->setCalendarPopup(true);
    dateEdit->setDisplayFormat("dd.MM.yyyy");
    if (auto *cal = dateEdit->calendarWidget()) {
        cal->setMaximumSize(320, 240);
        cal->setGridVisible(true);
    }

    auto *dayTypeLabel = new QLabel(&dialog);
    auto *slotCombo = new QComboBox(&dialog);

    auto *durationSpin = new QSpinBox(&dialog);
    durationSpin->setRange(15, 360);
    durationSpin->setSingleStep(15);
    durationSpin->setValue(90);

    auto *coachCombo = new QComboBox(&dialog);
    for (const auto &member : _club->getPlayers()) {
        auto coach = std::dynamic_pointer_cast<Coach>(member);
        if (coach) {
            coachCombo->addItem(QString("ID %1 · %2 (%3, кв. %4)")
                                    .arg(coach->getId())
                                    .arg(QString::fromStdString(coach->getFullName()))
                                    .arg(QString::fromStdString(coach->getSpecialization().toString()))
                                    .arg(coach->getQualificationLevel()),
                                coach->getId());
        }
    }

    if (coachCombo->count() == 0) {
        message("Сначала добавьте хотя бы одного тренера на вкладке «Тренеры».");
        return;
    }

    auto syncSlots = [dateEdit, dayTypeLabel, slotCombo](const QDate &d) {
        const bool weekend = isWeekendDate(d);
        dayTypeLabel->setText(weekend
            ? "Выходной день (сб–вс) — слоты: 10:00, 12:00, 14:00, 16:00"
            : "Будний день (пн–пт) — слоты: 17:00, 18:30, 20:00");

        const QString prev = slotCombo->currentText();
        slotCombo->clear();
        slotCombo->addItems(allowedSlotsForDate(d));
        const int idx = slotCombo->findText(prev);
        if (idx >= 0) {
            slotCombo->setCurrentIndex(idx);
        }
    };

    syncSlots(dateEdit->date());
    connect(dateEdit, &QDateEdit::dateChanged, &dialog, syncSlots);

    form->addRow("Название:", titleEdit);
    form->addRow("Тема:", topicEdit);
    form->addRow("Дата:", dateEdit);
    form->addRow("Расписание:", dayTypeLabel);
    form->addRow("Время тренировки:", slotCombo);
    form->addRow("Длительность (мин):", durationSpin);
    form->addRow("Тренер:", coachCombo);
    layout->addLayout(form);

    auto *buttons = new QDialogButtonBox(QDialogButtonBox::Ok | QDialogButtonBox::Cancel, &dialog);
    layout->addWidget(buttons);
    connect(buttons, &QDialogButtonBox::accepted, &dialog, [&]() {
        if (titleEdit->text().trimmed().isEmpty()) {
            QMessageBox::warning(&dialog, "Проверка", "Введите название тренировки.");
            return;
        }
        dialog.accept();
    });
    connect(buttons, &QDialogButtonBox::rejected, &dialog, &QDialog::reject);

    if (dialog.exec() != QDialog::Accepted) {
        return;
    }

    const int coachId = coachCombo->currentData().toInt();
    auto coach = std::dynamic_pointer_cast<Coach>(_club->findPlayerById(coachId));
    if (!coach) {
        message("Тренер не найден.");
        return;
    }

    const QString scheduleTag = QString("[%1 %2 · %3]")
                                  .arg(dateEdit->date().toString("dd.MM.yyyy"))
                                  .arg(slotCombo->currentText())
                                  .arg(isWeekendDate(dateEdit->date()) ? "выходной" : "будни");

    const QString combinedTopic = topicEdit->text().trimmed().isEmpty()
        ? scheduleTag
        : QString("%1 %2").arg(topicEdit->text().trimmed(), scheduleTag);

    try {
        const int id = _club->nextEventId();
        _club->addEvent(std::make_shared<TrainingSession>(
            id,
            titleEdit->text().trimmed().toStdString(),
            combinedTopic.toStdString(),
            durationSpin->value(),
            coach));

        logAction(QString("добавлена тренировка ID=%1 %2").arg(id).arg(scheduleTag));
        autoPersist();
        refreshTrainings();
        statusBar()->showMessage(QString("Тренировка добавлена, ID %1 %2").arg(id).arg(scheduleTag), 4000);
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

    if (isTrainingCompleted(id) || isTrainingCancelled(id)) {
        message("Удалить можно только запланированную тренировку.");
        return;
    }

    if (QMessageBox::question(this, "Удаление", QString("Удалить запланированную тренировку ID %1?").arg(id))
        != QMessageBox::Yes) {
        return;
    }

    if (_club->removeEvent(id)) {
        _completedTrainings.remove(id);
        _cancelledTrainings.remove(id);
        logAction(QString("удалена тренировка ID=%1").arg(id));
        autoPersist();
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

    if (isTrainingCompleted(trainingId) || isTrainingCancelled(trainingId)) {
        message("Нельзя менять состав завершённой или отменённой тренировки.");
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

    QDialog dialog(this);
    dialog.setWindowTitle("Запись участников на тренировку");
    dialog.resize(460, 380);

    auto *layout = new QVBoxLayout(&dialog);
    auto *headerLabel = new QLabel(&dialog);
    layout->addWidget(headerLabel);

    auto *currentList = new QListWidget(&dialog);
    layout->addWidget(new QLabel("Уже записаны:", &dialog));
    layout->addWidget(currentList, 1);

    auto *withdrawSelectedBtn = new QPushButton("Снять выбранного", &dialog);
    auto *listActionRow = new QHBoxLayout;
    listActionRow->addStretch();
    listActionRow->addWidget(withdrawSelectedBtn);
    layout->addLayout(listActionRow);

    auto *combo = new QComboBox(&dialog);
    layout->addWidget(new QLabel("Выберите участника для добавления:", &dialog));

    auto *addRow = new QHBoxLayout;
    auto *enrollBtn = new QPushButton("Записать", &dialog);
    addRow->addWidget(combo, 1);
    addRow->addWidget(enrollBtn);
    layout->addLayout(addRow);

    auto *closeBtn = new QPushButton("Закрыть", &dialog);
    auto *bottomRow = new QHBoxLayout;
    bottomRow->addStretch();
    bottomRow->addWidget(closeBtn);
    layout->addLayout(bottomRow);

    auto rebuildLists = [&]() {
        headerLabel->setText(QString("Тренировка: %1 (записано %2)")
                                 .arg(QString::fromStdString(std::string(training->getTitle())))
                                 .arg(training->getParticipants().size()));

        currentList->clear();
        for (const auto &member : training->getParticipants()) {
            if (member) {
                auto *item = new QListWidgetItem(QString("ID %1 · %2")
                                                    .arg(member->getId())
                                                    .arg(QString::fromStdString(member->getFullName())),
                                                currentList);
                item->setData(Qt::UserRole, member->getId());
            }
        }
        if (currentList->count() > 0) {
            currentList->setCurrentRow(0);
        }
        withdrawSelectedBtn->setEnabled(currentList->count() > 0);

        combo->clear();
        for (const auto &member : _club->getPlayers()) {
            if (!member) {
                continue;
            }
            bool already = false;
            for (const auto &p : training->getParticipants()) {
                if (p && p->getId() == member->getId()) {
                    already = true;
                    break;
                }
            }
            if (!already) {
                auto player = std::dynamic_pointer_cast<Player>(member);
                const QString roleTag = player
                    ? QString("рейтинг %1").arg(player->getRatings().getClassical())
                    : QString("тренер");
                combo->addItem(QString("ID %1 · %2 (%3)")
                                   .arg(member->getId())
                                   .arg(QString::fromStdString(member->getFullName()))
                                   .arg(roleTag),
                               member->getId());
            }
        }

        enrollBtn->setEnabled(combo->count() > 0);
    };

    rebuildLists();

    connect(enrollBtn, &QPushButton::clicked, &dialog, [&]() {
        if (combo->count() == 0) {
            return;
        }
        const int playerId = combo->currentData().toInt();
        bool added = _club->enrollPlayer(trainingId, playerId);
        if (!added) {
            if (auto member = _club->findPlayerById(playerId)) {
                *training += member;
                added = true;
            }
        }
        if (added) {
            logAction(QString("запись участника %1 на тренировку %2").arg(playerId).arg(trainingId));
            autoPersist();
            refreshTrainings();
            rebuildLists();
            statusBar()->showMessage(QString("Участник ID %1 записан.").arg(playerId), 4000);
        }
    });

    connect(withdrawSelectedBtn, &QPushButton::clicked, &dialog, [&]() {
        auto *item = currentList->currentItem();
        if (!item) {
            return;
        }
        const int playerId = item->data(Qt::UserRole).toInt();
        if (_club->withdrawPlayer(trainingId, playerId)) {
            logAction(QString("снятие участника %1 с тренировки %2").arg(playerId).arg(trainingId));
            autoPersist();
            refreshTrainings();
            rebuildLists();
            statusBar()->showMessage(QString("Участник ID %1 снят.").arg(playerId), 4000);
        }
    });

    connect(closeBtn, &QPushButton::clicked, &dialog, &QDialog::accept);
    dialog.exec();
}

void MainWindow::withdrawTrainingPlayer()
{
    const int trainingId = selectedId(_trainingsTab);
    if (trainingId < 0) {
        message("Выберите тренировку в таблице.");
        return;
    }

    if (isTrainingCompleted(trainingId) || isTrainingCancelled(trainingId)) {
        message("Нельзя менять состав завершённой или отменённой тренировки.");
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

    if (!training || training->getParticipants().empty()) {
        message("На эту тренировку пока никто не записан.");
        return;
    }

    QDialog dialog(this);
    dialog.setWindowTitle("Снятие участника с тренировки");
    dialog.resize(420, 280);

    auto *layout = new QVBoxLayout(&dialog);
    layout->addWidget(new QLabel(QString("Тренировка: %1")
                                     .arg(QString::fromStdString(std::string(training->getTitle()))),
                                 &dialog));
    layout->addWidget(new QLabel("Выберите участника для снятия:", &dialog));

    auto *listWidget = new QListWidget(&dialog);
    for (const auto &member : training->getParticipants()) {
        if (member) {
            auto *item = new QListWidgetItem(QString("ID %1 · %2")
                                                .arg(member->getId())
                                                .arg(QString::fromStdString(member->getFullName())),
                                            listWidget);
            item->setData(Qt::UserRole, member->getId());
        }
    }
    listWidget->setCurrentRow(0);
    layout->addWidget(listWidget, 1);

    auto *buttons = new QDialogButtonBox(QDialogButtonBox::Ok | QDialogButtonBox::Cancel, &dialog);
    layout->addWidget(buttons);
    connect(buttons, &QDialogButtonBox::accepted, &dialog, &QDialog::accept);
    connect(buttons, &QDialogButtonBox::rejected, &dialog, &QDialog::reject);

    if (dialog.exec() != QDialog::Accepted || !listWidget->currentItem()) {
        return;
    }

    const int playerId = listWidget->currentItem()->data(Qt::UserRole).toInt();
    if (_club->withdrawPlayer(trainingId, playerId)) {
        logAction(QString("снятие участника %1 с тренировки %2").arg(playerId).arg(trainingId));
        autoPersist();
        refreshTrainings();
        statusBar()->showMessage("Участник снят с тренировки.", 4000);
    } else {
        message("Не удалось снять участника.");
    }
}

void MainWindow::finishTraining()
{
    const int trainingId = selectedId(_trainingsTab);
    if (trainingId < 0) {
        message("Выберите тренировку в таблице.");
        return;
    }

    if (isTrainingCompleted(trainingId)) {
        message("Эта тренировка уже завершена и ELO уже начислено.");
        return;
    }

    if (isTrainingCancelled(trainingId)) {
        message("Отменённую тренировку нельзя завершить.");
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

    const int qual = training->getCoach() ? training->getCoach()->getQualificationLevel() : 3;
    const int bonusElo = std::clamp(10 + (training->getDurationMinutes() / 45) * 3 + std::max(0, qual - 3) * 2,
                                    10, 25);

    int updatedCount = 0;
    QStringList summaryLines;

    for (const auto &member : training->getParticipants()) {
        auto player = std::dynamic_pointer_cast<Player>(member);
        if (!player) {
            continue;
        }

        const Ratings oldR = player->getRatings();
        player->setRatings(Ratings(
            oldR.getClassical() + bonusElo,
            oldR.getRapid() + bonusElo,
            oldR.getBlitz() + bonusElo));
        ++updatedCount;
        summaryLines.push_back(QString("· %1: +%2 ELO (классика %3)")
                                   .arg(QString::fromStdString(player->getFullName()))
                                   .arg(bonusElo)
                                   .arg(player->getRatings().getClassical()));
    }

    _completedTrainings.insert(trainingId);
    logAction(QString("завершена тренировка ID=%1, начислено +%2 ELO игрокам (%3)")
                  .arg(trainingId).arg(bonusElo).arg(updatedCount));
    autoPersist();
    refreshAll();

    QString msg = QString("Тренировка «%1» завершена!\nНачислено +%2 ELO каждому игроку (%3 чел.)")
                      .arg(QString::fromStdString(std::string(training->getTitle())))
                      .arg(bonusElo)
                      .arg(updatedCount);
    if (!summaryLines.isEmpty()) {
        msg += "\n\n" + summaryLines.join("\n");
    }
    QMessageBox::information(this, "Тренировка завершена", msg);
}

void MainWindow::cancelTraining()
{
    const int trainingId = selectedId(_trainingsTab);
    if (trainingId < 0) {
        message("Выберите тренировку в таблице.");
        return;
    }

    if (isTrainingCompleted(trainingId)) {
        message("Нельзя отменить уже завершённую тренировку.");
        return;
    }

    if (isTrainingCancelled(trainingId)) {
        message("Эта тренировка уже отменена.");
        return;
    }

    if (QMessageBox::question(this, "Отмена тренировки",
                              QString("Отменить тренировку ID %1? ELO начисляться не будет.")
                                  .arg(trainingId))
        != QMessageBox::Yes) {
        return;
    }

    _cancelledTrainings.insert(trainingId);
    logAction(QString("отменена тренировка ID=%1").arg(trainingId));
    autoPersist();
    refreshTrainings();
    statusBar()->showMessage(QString("Тренировка ID %1 отменена.").arg(trainingId), 4000);
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

    auto *dialog = new QDialog(this);
    dialog->setWindowTitle(QString("Тренировка: %1")
                               .arg(QString::fromStdString(std::string(training->getTitle()))));
    dialog->resize(740, 500);

    auto *mainLayout = new QVBoxLayout(dialog);
    auto *summaryLabel = new QLabel(dialog);
    mainLayout->addWidget(summaryLabel);

    auto *subTabs = new QTabWidget(dialog);
    mainLayout->addWidget(subTabs, 1);

    auto *playersPage = new QWidget(subTabs);
    auto *playersLayout = new QVBoxLayout(playersPage);
    auto *playersTable = new QTableWidget(playersPage);
    playersTable->setColumnCount(3);
    playersTable->setHorizontalHeaderLabels({"ID", "Участник", "Рейтинг (классика)"});
    playersTable->setEditTriggers(QAbstractItemView::NoEditTriggers);
    playersTable->setSelectionBehavior(QAbstractItemView::SelectRows);
    playersTable->setSelectionMode(QAbstractItemView::SingleSelection);
    playersTable->verticalHeader()->setVisible(false);
    playersTable->horizontalHeader()->setStretchLastSection(true);
    playersLayout->addWidget(playersTable, 1);

    auto *playersBtnRow = new QHBoxLayout;
    auto *addPlayerBtn = new QPushButton("Записать...", playersPage);
    auto *removePlayerBtn = new QPushButton("Снять выбранного", playersPage);
    playersBtnRow->addWidget(addPlayerBtn);
    playersBtnRow->addWidget(removePlayerBtn);
    playersBtnRow->addStretch();
    playersLayout->addLayout(playersBtnRow);
    subTabs->addTab(playersPage, "Участники");

    auto *detailsPage = new QWidget(subTabs);
    auto *detailsLayout = new QVBoxLayout(detailsPage);
    auto *detailsView = new QTextEdit(detailsPage);
    detailsView->setReadOnly(true);
    detailsLayout->addWidget(detailsView, 1);
    subTabs->addTab(detailsPage, "Детали");

    auto reloadTrainingView = [&]() {
        const bool locked = isTrainingCompleted(trainingId) || isTrainingCancelled(trainingId);
        const QString statusText = trainingStatusLabel(trainingId);
        const QString coachName = training->getCoach()
            ? QString::fromStdString(training->getCoach()->getFullName())
            : QString("не назначен");
        const int qual = training->getCoach() ? training->getCoach()->getQualificationLevel() : 3;
        const int bonusElo = std::clamp(10 + (training->getDurationMinutes() / 45) * 3 + std::max(0, qual - 3) * 2,
                                        10, 25);

        addPlayerBtn->setEnabled(!locked);
        removePlayerBtn->setEnabled(!locked);

        summaryLabel->setText(
            QString("<b>%1</b> (ID %2) · Тренер: %3 · Длительность: %4 мин · Участники: %5 · Статус: <b>%6</b>")
                .arg(QString::fromStdString(std::string(training->getTitle())))
                .arg(trainingId)
                .arg(coachName)
                .arg(training->getDurationMinutes())
                .arg(training->getParticipants().size())
                .arg(statusText));

        int enrolledCount = 0;
        for (const auto &member : training->getParticipants()) {
            if (member) {
                ++enrolledCount;
            }
        }
        playersTable->setRowCount(enrolledCount);

        int row = 0;
        for (const auto &member : training->getParticipants()) {
            if (!member) continue;
            const int rating = member->getRatings().getClassical();
            auto *idItem = new QTableWidgetItem(QString::number(member->getId()));
            idItem->setData(Qt::UserRole, member->getId());

            playersTable->setItem(row, 0, idItem);
            playersTable->setItem(row, 1, new QTableWidgetItem(QString::fromStdString(member->getFullName())));
            playersTable->setItem(row, 2, new QTableWidgetItem(QString::number(rating)));
            ++row;
        }
        playersTable->resizeColumnsToContents();

        QString detailsText = QString("Название: %1 (ID %2)\n"
                                       "Статус: %3\n"
                                       "Тема и расписание: %4\n"
                                       "Тренер: %5\n"
                                       "Длительность: %6 мин\n"
                                       "Бонус при завершении: +%7 ELO\n"
                                       "Количество участников: %8")
                                  .arg(QString::fromStdString(std::string(training->getTitle())))
                                  .arg(trainingId)
                                  .arg(statusText)
                                  .arg(QString::fromStdString(std::string(training->getTopic())))
                                  .arg(coachName)
                                  .arg(training->getDurationMinutes())
                                  .arg(bonusElo)
                                  .arg(enrolledCount);
        detailsView->setPlainText(detailsText);
    };

    reloadTrainingView();

    connect(addPlayerBtn, &QPushButton::clicked, dialog, [&]() {
        if (isTrainingCompleted(trainingId) || isTrainingCancelled(trainingId)) {
            QMessageBox::information(dialog, "Тренировка недоступна для изменений",
                                     "Нельзя менять состав завершённой или отменённой тренировки.");
            return;
        }

        QDialog addDlg(dialog);
        addDlg.setWindowTitle("Записать участника на тренировку");
        addDlg.resize(400, 170);

        auto *lay = new QVBoxLayout(&addDlg);
        lay->addWidget(new QLabel("Выберите участника:", &addDlg));
        auto *combo = new QComboBox(&addDlg);

        for (const auto &member : _club->getPlayers()) {
            if (!member) continue;
            bool already = false;
            for (const auto &p : training->getParticipants()) {
                if (p && p->getId() == member->getId()) {
                    already = true;
                    break;
                }
            }
            if (!already) {
                combo->addItem(QString("ID %1 · %2 (рейтинг %3)")
                                   .arg(member->getId())
                                   .arg(QString::fromStdString(member->getFullName()))
                                   .arg(member->getRatings().getClassical()),
                               member->getId());
            }
        }

        if (combo->count() == 0) {
            QMessageBox::information(dialog, "Запись", "Все участники клуба уже записаны на эту тренировку.");
            return;
        }

        lay->addWidget(combo);
        auto *btns = new QDialogButtonBox(QDialogButtonBox::Ok | QDialogButtonBox::Cancel, &addDlg);
        lay->addWidget(btns);
        connect(btns, &QDialogButtonBox::accepted, &addDlg, &QDialog::accept);
        connect(btns, &QDialogButtonBox::rejected, &addDlg, &QDialog::reject);

        if (addDlg.exec() != QDialog::Accepted) {
            return;
        }

        const int pid = combo->currentData().toInt();
        bool added = _club->enrollPlayer(trainingId, pid);
        if (!added) {
            if (auto member = _club->findPlayerById(pid)) {
                *training += member;
                added = true;
            }
        }
        if (added) {
            logAction(QString("запись участника %1 на тренировку %2").arg(pid).arg(trainingId));
            autoPersist();
            refreshTrainings();
            reloadTrainingView();
            statusBar()->showMessage(QString("Участник ID %1 записан.").arg(pid), 4000);
        }
    });

    connect(removePlayerBtn, &QPushButton::clicked, dialog, [&]() {
        if (isTrainingCompleted(trainingId) || isTrainingCancelled(trainingId)) {
            QMessageBox::information(dialog, "Тренировка недоступна для изменений",
                                     "Нельзя менять состав завершённой или отменённой тренировки.");
            return;
        }
        const int row = playersTable->currentRow();
        if (row < 0 || !playersTable->item(row, 0)) {
            message("Выберите участника в списке.");
            return;
        }
        const int pid = playersTable->item(row, 0)->data(Qt::UserRole).toInt();
        if (_club->withdrawPlayer(trainingId, pid)) {
            logAction(QString("снятие участника %1 с тренировки %2").arg(pid).arg(trainingId));
            autoPersist();
            refreshTrainings();
            reloadTrainingView();
            statusBar()->showMessage(QString("Участник ID %1 снят.").arg(pid), 4000);
        }
    });

    auto *closeRow = new QHBoxLayout;
    auto *closeBtn = new QPushButton("Закрыть", dialog);
    closeRow->addStretch();
    closeRow->addWidget(closeBtn);
    mainLayout->addLayout(closeRow);
    connect(closeBtn, &QPushButton::clicked, dialog, &QDialog::accept);

    dialog->exec();
    dialog->deleteLater();
}

void MainWindow::addMatch()
{
    CreateMatchDialog dialog(this);

    QComboBox *tournamentCombo = nullptr;
    for (auto *spin : dialog.findChildren<QSpinBox *>()) {
        if (spin->specialValueText().contains("вне турнира", Qt::CaseInsensitive)) {
            auto *combo = new QComboBox(&dialog);
            combo->addItem("Вне турнира", 0);
            for (const auto &event : _club->getEvents()) {
                auto tournament = std::dynamic_pointer_cast<Tournament>(event);
                if (tournament && !isTournamentFinished(tournament->getId())) {
                    combo->addItem(QString("ID %1 · %2")
                                       .arg(tournament->getId())
                                       .arg(QString::fromStdString(std::string(tournament->getTitle()))),
                                   tournament->getId());
                }
            }
            if (auto *form = qobject_cast<QFormLayout *>(spin->parentWidget()->layout())) {
                form->replaceWidget(spin, combo);
            } else if (auto *parentLayout = spin->parentWidget()->layout()) {
                parentLayout->replaceWidget(spin, combo);
            }
            spin->hide();
            tournamentCombo = combo;
            break;
        }
    }

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

    if (dialog.whiteId == dialog.blackId) {
        message("Игрок не может сыграть партию сам с собой. Выберите разных игроков.");
        return;
    }

    const int tournamentId = tournamentCombo ? tournamentCombo->currentData().toInt() : dialog.tournamentId;
    if (tournamentId > 0 && isTournamentFinished(tournamentId)) {
        message("Турнир уже завершён — добавлять в него партии нельзя.");
        return;
    }

    Tournament::TimeControl chosenControl = Tournament::TimeControl::Rapid;
    if (tournamentId == 0) {
        bool ok = false;
        const QStringList controls = {"Блиц (3:00)", "Рапид (10:00)", "Классика (30:00)"};
        const QString pick = QInputDialog::getItem(
            this, "Контроль времени", "Выберите контроль времени для партии:",
            controls, 1, false, &ok);
        if (!ok) {
            return;
        }
        if (pick.startsWith("Блиц")) chosenControl = Tournament::TimeControl::Blitz;
        else if (pick.startsWith("Классика")) chosenControl = Tournament::TimeControl::Classic;
        else chosenControl = Tournament::TimeControl::Rapid;
    }

    const int id = _club->nextMatchId();
    const DateTime date(dialog.date.day(), dialog.date.month(), dialog.date.year(),
                        dialog.time.hour(), dialog.time.minute(), 0);

    try {
        if (tournamentId > 0) {
            if (!_club->createMatchInTournament(id, tournamentId,
                                                dialog.whiteId, dialog.blackId, date)) {
                message("Не удалось: турнир не найден или оба игрока не записаны.");
                return;
            }
            if (auto created = _club->findMatchById(id)) {
                created->setTournamentId(tournamentId);
            }
        } else {
            auto white = std::dynamic_pointer_cast<Player>(_club->findPlayerById(dialog.whiteId));
            auto black = std::dynamic_pointer_cast<Player>(_club->findPlayerById(dialog.blackId));
            if (!white || !black) {
                message("Игроки не найдены.");
                return;
            }
            _club->createMatch(id, white, black, date);
            standaloneMatchControls().insert(id, chosenControl);
        }

        logAction(QString("создана партия ID=%1").arg(id));
        autoPersist();
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
    if (match->getResult().isPlayed()) {
        message("Этот матч уже сыгран — сыграть его заново нельзя.");
        return;
    }
    if (match->getWhitePlayer() && match->getBlackPlayer()
        && match->getWhitePlayer()->getId() == match->getBlackPlayer()->getId()) {
        message("Некорректная партия: игрок не может играть сам с собой.");
        return;
    }
    if (!match->hasGame()) {
        message("У этой партии нет истории ходов (она импортирована из старого формата).");
        return;
    }

    Tournament::TimeControl control = Tournament::TimeControl::Rapid;
    if (match->getTournamentId() > 0) {
        if (auto tournament = _club->findTournamentById(match->getTournamentId())) {
            control = tournament->getTimeControl();
        }
    } else if (standaloneMatchControls().contains(id)) {
        control = standaloneMatchControls().value(id);
    }

    GameDialog dialog(_club.get(), id, control, this);
    dialog.exec();
    autoPersist();
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
        autoPersist();
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

    auto match = _club->findMatchById(id);
    if (!match) {
        message("Партия не найдена.");
        return;
    }

    if (match->getResult().isPlayed()) {
        message("Сыгранную партию удалить нельзя — можно удалить только запланированную.");
        return;
    }

    if (match->getTournamentId() > 0) {
        message("Партию, привязанную к турниру, удалить нельзя — можно удалить только запланированную вне турнира.");
        return;
    }

    if (QMessageBox::question(this, "Удаление", QString("Удалить запланированную партию ID %1?").arg(id))
        != QMessageBox::Yes) {
        return;
    }

    if (_club->removeMatch(id)) {
        standaloneMatchControls().remove(id);
        logAction(QString("удалена партия ID=%1").arg(id));
        autoPersist();
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
        StorageManager::generateReport(filePath(REPORT_FILE).toStdString(), *_club);
        logAction("сохранение состояния и отчёта");
        statusBar()->showMessage("Сохранено в club.txt и report.txt", 4000);
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

void MainWindow::applyBlueTheme()
{
    // Цветовая схема в стиле серо-сиреневого / пыльно-лавандового референса.
    const QString style = R"(
QWidget { background-color: #2e303a; color: #f2f1f7; font-size: 13px; }
QMainWindow, QDialog, QMessageBox { background-color: #2e303a; }
QTableView {
    background-color: #383b48;
    gridline-color: #525669;
    selection-background-color: #b694b4;
    selection-color: #23242c;
    border: 1px solid #525669;
    border-radius: 10px;
}
QHeaderView::section {
    background-color: #454858;
    color: #f2f1f7;
    border: 1px solid #585c70;
    padding: 7px 10px;
    font-weight: 600;
}
QLineEdit, QSpinBox, QComboBox, QDateEdit, QTimeEdit, QListWidget {
    background-color: #383b48;
    color: #f2f1f7;
    border: 1px solid #585c70;
    border-radius: 10px;
    padding: 7px 11px;
    min-height: 18px;
}
QLineEdit:focus, QSpinBox:focus, QComboBox:focus, QDateEdit:focus, QTimeEdit:focus, QListWidget:focus {
    border-color: #c9a8c7;
}
QPushButton {
    background-color: #9f7e9e;
    color: #f7f6fb;
    border: 1px solid #b594b4;
    border-radius: 12px;
    padding: 10px 18px;
    font-weight: 600;
    min-height: 20px;
}
QPushButton:hover {
    background-color: #b895b7;
    border-color: #d2b2d1;
}
QPushButton:pressed {
    background-color: #846583;
}
QPushButton:disabled {
    background-color: #3d404e;
    border-color: #4b4e60;
    color: #9194a6;
}
QTabWidget::pane {
    border: 1px solid #525669;
    border-radius: 10px;
    background-color: #2e303a;
}
QTabBar::tab {
    background: #383b48;
    color: #e4e2ee;
    border: 1px solid #525669;
    border-top-left-radius: 10px;
    border-top-right-radius: 10px;
    padding: 8px 18px;
    margin-right: 4px;
}
QTabBar::tab:selected {
    background: #b694b4;
    color: #23242c;
    font-weight: 600;
}
QMenuBar, QMenu {
    background-color: #383b48;
    color: #f2f1f7;
}
QMenuBar::item:selected, QMenu::item:selected {
    background: #b694b4;
    color: #23242c;
}
QToolTip {
    background-color: #383b48;
    color: #f2f1f7;
    border: 1px solid #c9a8c7;
    border-radius: 8px;
    padding: 6px;
}
QStatusBar {
    background-color: #383b48;
    color: #e4e2ee;
    border-top: 1px solid #525669;
}
)";

    qApp->setStyleSheet(style);
}