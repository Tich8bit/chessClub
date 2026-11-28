#include "gamedialog.h"

#include <QAction>
#include <QActionGroup>
#include <QComboBox>
#include <QHBoxLayout>
#include <QInputDialog>
#include <QLabel>
#include <QListWidget>
#include <QMenu>
#include <QMenuBar>
#include <QMessageBox>
#include <QPushButton>
#include <QSignalBlocker>
#include <QTimer>
#include <QVBoxLayout>

#include <algorithm>
#include <utility>

#include "chessClub.h"
#include "chessboardwidget.h"
#include "chessgame.h"
#include "exceptions/exception.h"
#include "match.h"

namespace {

template <typename T, typename Parent, typename... Args>
T *makeOwned(Parent *parent, Args &&...args)
{
    auto ptr = std::make_unique<T>(std::forward<Args>(args)..., parent);
    return ptr.release();
}

template <typename T, typename... Args>
T *makeLayout(Args &&...args)
{
    auto ptr = std::make_unique<T>(std::forward<Args>(args)...);
    return ptr.release();
}

QString squareName(int square)
{
    if (square < 0 || square >= 64) {
        return QString();
    }
    return QString(QChar('a' + square % 8)) + QString::number(square / 8 + 1);
}

int squareIndex(const QString &name)
{
    if (name.size() < 2) {
        return -1;
    }
    const int file = name[0].toLatin1() - 'a';
    const int rank = name[1].toLatin1() - '1';
    if (file < 0 || file > 7 || rank < 0 || rank > 7) {
        return -1;
    }
    return rank * 8 + file;
}

QString controlLabel(Tournament::TimeControl control)
{
    switch (control) {
    case Tournament::TimeControl::Blitz:   return "Блиц";
    case Tournament::TimeControl::Rapid:   return "Рапид";
    case Tournament::TimeControl::Classic: return "Классика";
    }
    return "Рапид";
}

} // namespace

int GameDialog::initialSecondsForControl(Tournament::TimeControl control)
{
    switch (control) {
    case Tournament::TimeControl::Blitz:   return 3 * 60;
    case Tournament::TimeControl::Rapid:   return 10 * 60;
    case Tournament::TimeControl::Classic: return 30 * 60;
    }
    return 10 * 60;
}

QString GameDialog::formatSeconds(int seconds)
{
    const int clamped = std::max(0, seconds);
    const int mins = clamped / 60;
    const int secs = clamped % 60;
    return QString("%1:%2")
        .arg(mins, 2, 10, QChar('0'))
        .arg(secs, 2, 10, QChar('0'));
}

GameDialog::GameDialog(ChessClub *club,
                       int matchId,
                       Tournament::TimeControl timeControl,
                       QWidget *parent)
    : QDialog(parent)
    , _club(club)
    , _matchId(matchId)
    , _timeControl(timeControl)
{
    auto match = _club ? _club->findMatchById(_matchId) : nullptr;
    _game = match ? match->game() : nullptr;
    _editable = match && _game && !match->getResult().isPlayed();
    _ply = _game ? static_cast<int>(_game->history().size()) : 0;

    const int initSec = initialSecondsForControl(_timeControl);
    _whiteSecondsLeft = initSec;
    _blackSecondsLeft = initSec;

    setWindowTitle(QString("Партия #%1 · %2").arg(matchId).arg(controlLabel(_timeControl)));
    resize(1140, 790);

    setStyleSheet(R"(
QDialog { background-color: #2e303a; }
QPushButton {
    background-color: #9f7e9e;
    color: #f7f6fb;
    font-weight: 600;
    font-size: 13px;
    border: 1px solid #b594b4;
    border-radius: 12px;
    padding: 10px 18px;
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
QComboBox {
    background-color: #383b48;
    color: #f2f1f7;
    border: 1px solid #585c70;
    border-radius: 10px;
    padding: 7px 11px;
    min-height: 20px;
}
QListWidget {
    background-color: #383b48;
    color: #f2f1f7;
    border: 1px solid #525669;
    border-radius: 10px;
    padding: 6px;
}
QLabel { color: #f2f1f7; }
QMenuBar { background-color: #383b48; color: #f2f1f7; }
QMenuBar::item:selected { background-color: #9f7e9e; color: #ffffff; }
QMenu { background-color: #383b48; color: #f2f1f7; border: 1px solid #525669; }
QMenu::item:selected { background-color: #9f7e9e; color: #ffffff; }
)");

    auto *menuBar = makeOwned<QMenuBar>(this);
    auto *viewMenu = menuBar->addMenu("Вид");
    auto *group = makeOwned<QActionGroup>(this);
    group->setExclusive(true);

    _whiteAction = viewMenu->addAction("Белые снизу");
    _whiteAction->setCheckable(true);
    _whiteAction->setChecked(true);
    group->addAction(_whiteAction);

    _blackAction = viewMenu->addAction("Чёрные снизу");
    _blackAction->setCheckable(true);
    group->addAction(_blackAction);

    _autoFlipAction = viewMenu->addAction("Переворачивать после хода");
    _autoFlipAction->setCheckable(true);
    group->addAction(_autoFlipAction);

    connect(_whiteAction, &QAction::triggered, this, &GameDialog::setWhiteBottomMode);
    connect(_blackAction, &QAction::triggered, this, &GameDialog::setBlackBottomMode);
    connect(_autoFlipAction, &QAction::triggered, this, &GameDialog::setAutoFlipMode);

    _board = makeOwned<ChessBoardWidget>(this);
    _moveList = makeOwned<QListWidget>(this);
    _moveList->setMinimumWidth(260);

    _whiteClockLabel = makeOwned<QLabel>(this);
    _blackClockLabel = makeOwned<QLabel>(this);

    auto *clocksRow = makeLayout<QHBoxLayout>();
    clocksRow->setSpacing(10);
    clocksRow->addWidget(_whiteClockLabel, 1);
    clocksRow->addWidget(_blackClockLabel, 1);

    _status = makeOwned<QLabel>(this);
    _status->setWordWrap(true);

    _undoButton = makeOwned<QPushButton>(this, "Отменить ход");
    _resignButton = makeOwned<QPushButton>(this, "Сдаться");
    _drawButton = makeOwned<QPushButton>(this, "Ничья");
    auto *closeButton = makeOwned<QPushButton>(this, "Закрыть");

    _startButton = makeOwned<QPushButton>(this, "|<");
    _prevButton = makeOwned<QPushButton>(this, "<");
    _nextButton = makeOwned<QPushButton>(this, ">");
    _endButton = makeOwned<QPushButton>(this, ">|");

    auto *navLayout = makeLayout<QHBoxLayout>();
    navLayout->setSpacing(8);
    navLayout->addWidget(_startButton);
    navLayout->addWidget(_prevButton);
    navLayout->addWidget(_nextButton);
    navLayout->addWidget(_endButton);

    _autoPlayButton = makeOwned<QPushButton>(this, "Авто");
    _intervalCombo = makeOwned<QComboBox>(this);
    _intervalCombo->addItem("0.5 сек", 500);
    _intervalCombo->addItem("1 сек", 1000);
    _intervalCombo->addItem("1.5 сек", 1500);
    _intervalCombo->addItem("2 сек", 2000);
    _intervalCombo->setCurrentIndex(1);

    auto *autoLayout = makeLayout<QHBoxLayout>();
    autoLayout->setSpacing(8);
    autoLayout->addWidget(_autoPlayButton, 1);
    autoLayout->addWidget(makeOwned<QLabel>(this, "Интервал:"));
    autoLayout->addWidget(_intervalCombo);

    auto *right = makeLayout<QVBoxLayout>();
    right->setSpacing(10);
    right->addWidget(makeOwned<QLabel>(this, QString("Контроль: %1").arg(controlLabel(_timeControl))));
    right->addLayout(clocksRow);
    right->addWidget(makeOwned<QLabel>(this, "Ходы:"));
    right->addWidget(_moveList, 1);
    right->addLayout(navLayout);
    right->addLayout(autoLayout);
    right->addWidget(_status);
    right->addWidget(_undoButton);
    right->addWidget(_resignButton);
    right->addWidget(_drawButton);
    right->addWidget(closeButton);

    auto *content = makeLayout<QHBoxLayout>();
    content->setSpacing(16);
    content->addWidget(_board, 3);
    content->addLayout(right, 1);

    auto *root = makeLayout<QVBoxLayout>(this);
    root->setMenuBar(menuBar);
    root->addLayout(content);

    _clockTimer = makeOwned<QTimer>(this);
    _clockTimer->setInterval(1000);
    connect(_clockTimer, &QTimer::timeout, this, &GameDialog::onClockTick);

    _autoPlayTimer = makeOwned<QTimer>(this);
    _autoPlayTimer->setInterval(_intervalCombo->currentData().toInt());
    connect(_autoPlayTimer, &QTimer::timeout, this, &GameDialog::onAutoPlayTick);

    connect(_board, &ChessBoardWidget::squareClicked, this, &GameDialog::onSquareClicked);
    connect(_undoButton, &QPushButton::clicked, this, &GameDialog::undoMove);
    connect(_resignButton, &QPushButton::clicked, this, &GameDialog::resign);
    connect(_drawButton, &QPushButton::clicked, this, &GameDialog::drawAgree);
    connect(closeButton, &QPushButton::clicked, this, &GameDialog::accept);

    connect(_startButton, &QPushButton::clicked, this, &GameDialog::toStart);
    connect(_prevButton, &QPushButton::clicked, this, &GameDialog::stepBack);
    connect(_nextButton, &QPushButton::clicked, this, &GameDialog::stepForward);
    connect(_endButton, &QPushButton::clicked, this, &GameDialog::toEnd);
    connect(_autoPlayButton, &QPushButton::clicked, this, &GameDialog::toggleAutoPlay);
    connect(_intervalCombo, &QComboBox::currentIndexChanged, this, &GameDialog::onAutoIntervalChanged);
    connect(_moveList, &QListWidget::currentRowChanged, this, &GameDialog::onMoveActivated);

    refresh();
    syncClockTimer();
}

GameDialog::~GameDialog()
{
    if (_clockTimer) {
        _clockTimer->stop();
    }
    stopAutoPlay();
}

void GameDialog::stopAutoPlay()
{
    _autoPlaying = false;
    if (_autoPlayTimer && _autoPlayTimer->isActive()) {
        _autoPlayTimer->stop();
    }
    if (_autoPlayButton) {
        _autoPlayButton->setText("Авто");
    }
}

void GameDialog::toggleAutoPlay()
{
    if (_editable || !_game) {
        return;
    }

    if (_autoPlaying) {
        stopAutoPlay();
        return;
    }

    const int total = static_cast<int>(_game->history().size());
    if (total == 0) {
        return;
    }

    if (_ply >= total) {
        _ply = 0;
        refresh();
    }

    _autoPlaying = true;
    _autoPlayButton->setText("Пауза");
    if (_autoPlayTimer) {
        _autoPlayTimer->setInterval(_intervalCombo ? _intervalCombo->currentData().toInt() : 1000);
        _autoPlayTimer->start();
    }
}

void GameDialog::onAutoPlayTick()
{
    if (_editable || !_game || !_autoPlaying) {
        stopAutoPlay();
        return;
    }

    const int total = static_cast<int>(_game->history().size());
    if (_ply < total) {
        ++_ply;
        refresh();
    }

    if (_ply >= total) {
        stopAutoPlay();
    }
}

void GameDialog::onAutoIntervalChanged(int index)
{
    Q_UNUSED(index);
    if (!_intervalCombo || !_autoPlayTimer) {
        return;
    }
    const int ms = _intervalCombo->currentData().toInt();
    if (ms > 0) {
        _autoPlayTimer->setInterval(ms);
    }
}

void GameDialog::updateClocks()
{
    if (!_whiteClockLabel || !_blackClockLabel) {
        return;
    }

    const bool activeWhite = _editable && !_flagged && _game && !_game->isOver() && _game->whiteToMove();
    const bool activeBlack = _editable && !_flagged && _game && !_game->isOver() && !_game->whiteToMove();

    _whiteClockLabel->setText(QString("Белые: %1").arg(formatSeconds(_whiteSecondsLeft)));
    _blackClockLabel->setText(QString("Чёрные: %1").arg(formatSeconds(_blackSecondsLeft)));

    const QString activeStyle =
        "background-color: #464259; border: 2px solid #d2b2d1; border-radius: 10px; "
        "padding: 8px 10px; font-weight: 700; font-size: 14px; color: #ffffff;";
    const QString idleStyle =
        "background-color: #383b48; border: 1px solid #525669; border-radius: 10px; "
        "padding: 8px 10px; font-weight: 600; font-size: 14px; color: #d6d4e2;";

    _whiteClockLabel->setStyleSheet(activeWhite ? activeStyle : idleStyle);
    _blackClockLabel->setStyleSheet(activeBlack ? activeStyle : idleStyle);
}

void GameDialog::syncClockTimer()
{
    if (!_clockTimer) {
        return;
    }
    const bool shouldRun = _editable && !_flagged && _game && !_game->isOver();
    if (shouldRun && !_clockTimer->isActive()) {
        _clockTimer->start();
    } else if (!shouldRun && _clockTimer->isActive()) {
        _clockTimer->stop();
    }
}

void GameDialog::onClockTick()
{
    if (!_editable || _flagged || !_game || _game->isOver()) {
        syncClockTimer();
        return;
    }

    if (_game->whiteToMove()) {
        _whiteSecondsLeft = std::max(0, _whiteSecondsLeft - 1);
    } else {
        _blackSecondsLeft = std::max(0, _blackSecondsLeft - 1);
    }

    updateClocks();

    if (_whiteSecondsLeft <= 0 || _blackSecondsLeft <= 0) {
        _flagged = true;
        syncClockTimer();

        const bool whiteTimedOut = (_whiteSecondsLeft <= 0);
        if (_club) {
            _club->resignMatch(_matchId, whiteTimedOut);
        }
        _editable = false;
        clearSelection();
        refresh();

        QMessageBox::information(
            this,
            "Время вышло",
            whiteTimedOut
                ? "У белых закончилось время. Победа чёрных по времени."
                : "У чёрных закончилось время. Победа белых по времени.");
    }
}

void GameDialog::rebuildView()
{
    if (!_game) {
        return;
    }

    if (_editable) {
        _viewGame = _game;
    } else {
        _viewGame = std::make_shared<ChessGame>();
        const auto &history = _game->history();
        for (int i = 0; i < _ply && i < static_cast<int>(history.size()); ++i) {
            _viewGame->makeMove(history[i]);
        }
    }

    _board->setGame(_viewGame.get());
}

void GameDialog::syncBoardOrientation()
{
    switch (_orientationMode) {
    case OrientationMode::WhiteBottom:
        _flipped = false;
        break;
    case OrientationMode::BlackBottom:
        _flipped = true;
        break;
    case OrientationMode::AutoFlip:
        if (_editable && _game && !_game->isOver()) {
            _flipped = !_game->whiteToMove();
        } else if (!_editable) {
            _flipped = (_ply % 2) != 0;
        } else {
            _flipped = false;
        }
        break;
    }

    if (_whiteAction && _blackAction && _autoFlipAction) {
        QSignalBlocker b1(_whiteAction);
        QSignalBlocker b2(_blackAction);
        QSignalBlocker b3(_autoFlipAction);
        _whiteAction->setChecked(_orientationMode == OrientationMode::WhiteBottom);
        _blackAction->setChecked(_orientationMode == OrientationMode::BlackBottom);
        _autoFlipAction->setChecked(_orientationMode == OrientationMode::AutoFlip);
    }

    _board->setFlipped(_flipped);
}

void GameDialog::refresh()
{
    if (!_game) {
        _status->setText("У этой партии нет истории ходов.");
        updateClocks();
        syncClockTimer();
        return;
    }

    const auto &history = _game->history();
    if (_editable) {
        _ply = static_cast<int>(history.size());
    }
    _ply = std::max(0, std::min(_ply, static_cast<int>(history.size())));

    rebuildView();
    syncBoardOrientation();

    _board->setSelectedSquare(_selected);
    _board->setLegalTargets(_targets);

    if (_ply > 0) {
        const QString last = QString::fromStdString(history[_ply - 1]);
        _board->setLastMove(squareIndex(last.left(2)), squareIndex(last.mid(2, 2)));
    } else {
        _board->setLastMove(-1, -1);
    }

    _board->setCheckSquare(_viewGame && _viewGame->inCheck()
                               ? _viewGame->kingSquare(_viewGame->whiteToMove())
                               : -1);

    QListWidget *list = _moveList;
    const QSignalBlocker blocker(list);
    list->clear();
    for (size_t i = 0; i < history.size(); ++i) {
        list->addItem(QString("%1. %2").arg(i + 1).arg(QString::fromStdString(history[i])));
    }
    if (!_editable && _ply > 0) {
        list->setCurrentRow(_ply - 1);
    } else if (_editable) {
        list->scrollToBottom();
    }

    if (!_editable) {
        const auto match = _club ? _club->findMatchById(_matchId) : nullptr;
        QString result;
        if (match && match->getResult().isPlayed()) {
            result = QString(" · результат: %1").arg(QString::fromStdString(match->getResult().toString()));
        }
        const QString autoTag = _autoPlaying ? " · автопросмотр" : QString();
        _status->setText(QString("Просмотр: ход %1 из %2%3%4")
                             .arg(_ply)
                             .arg(history.size())
                             .arg(result)
                             .arg(autoTag));
    } else if (_game->isOver()) {
        _status->setText("Партия окончена.");
    } else {
        QString text = _game->whiteToMove() ? "Ход белых." : "Ход чёрных.";
        if (_game->inCheck()) {
            text += " Шах!";
        }
        _status->setText(text);
    }

    _undoButton->setEnabled(_editable && !_game->isOver() && !history.empty());
    _resignButton->setEnabled(_editable && !_game->isOver());
    _drawButton->setEnabled(_editable && !_game->isOver());

    updateClocks();
    syncClockTimer();
    updateNavigation();
}

void GameDialog::updateNavigation()
{
    if (!_game) {
        return;
    }

    const int total = static_cast<int>(_game->history().size());
    const bool reviewButtons = !_editable;

    _startButton->setEnabled(reviewButtons && _ply > 0);
    _prevButton->setEnabled(reviewButtons && _ply > 0);
    _nextButton->setEnabled(reviewButtons && _ply < total);
    _endButton->setEnabled(reviewButtons && _ply < total);

    _startButton->setVisible(reviewButtons);
    _prevButton->setVisible(reviewButtons);
    _nextButton->setVisible(reviewButtons);
    _endButton->setVisible(reviewButtons);

    _autoPlayButton->setVisible(reviewButtons);
    _intervalCombo->setVisible(reviewButtons);
    _autoPlayButton->setEnabled(reviewButtons && total > 0);
    _intervalCombo->setEnabled(reviewButtons);
}

void GameDialog::clearSelection()
{
    _selected = -1;
    _targets.clear();
}

QString GameDialog::askPromotion() const
{
    const bool wasRunning = _clockTimer && _clockTimer->isActive();
    if (wasRunning) {
        _clockTimer->stop();
    }

    const QStringList options{"Ферзь", "Ладья", "Слон", "Конь"};
    bool ok = false;
    const QString choice = QInputDialog::getItem(const_cast<GameDialog *>(this), "Превращение",
                                                 "Выберите фигуру:", options, 0, false, &ok);

    if (wasRunning && _clockTimer) {
        _clockTimer->start();
    }

    if (!ok) {
        return QString();
    }

    if (choice == "Ферзь") return "q";
    if (choice == "Ладья") return "r";
    if (choice == "Слон") return "b";
    return "n";
}

void GameDialog::onSquareClicked(int square)
{
    if (!_editable || !_game || _game->isOver()) {
        return;
    }

    if (_selected >= 0 && _targets.contains(square)) {
        const QString from = squareName(_selected);
        const QString to = squareName(square);

        QString uci;
        for (const auto &candidate : _game->legalMoves()) {
            const QString text = QString::fromStdString(candidate);
            if (text.startsWith(from + to)) {
                uci = text;
                break;
            }
        }

        if (!uci.isEmpty()) {
            if (uci.size() == 5) {
                const QString promotion = askPromotion();
                if (promotion.isEmpty()) {
                    clearSelection();
                    refresh();
                    return;
                }
                uci = uci.left(4) + promotion;
            }

            try {
                if (_club) {
                    _club->playMove(_matchId, uci.toStdString());
                }
            } catch (const ChessException &e) {
                QMessageBox::warning(this, "Ход", e.what());
            }
        }

        clearSelection();
        refresh();
        return;
    }

    const char piece = _game->pieceAt(square);
    const bool whitePiece = piece >= 'A' && piece <= 'Z';

    if (piece != '.' && whitePiece == _game->whiteToMove()) {
        _selected = square;
        _targets.clear();

        const QString from = squareName(square);
        for (const auto &candidate : _game->legalMoves()) {
            const QString text = QString::fromStdString(candidate);
            if (text.startsWith(from)) {
                _targets.insert(squareIndex(text.mid(2, 2)));
            }
        }
    } else {
        clearSelection();
    }

    refresh();
}

void GameDialog::undoMove()
{
    if (_club && _club->undoMove(_matchId)) {
        clearSelection();
        refresh();
    }
}

void GameDialog::resign()
{
    const bool wasRunning = _clockTimer && _clockTimer->isActive();
    if (wasRunning) {
        _clockTimer->stop();
    }

    const bool whiteResigns = _game->whiteToMove();
    const QString who = whiteResigns ? "белые" : "чёрные";

    if (QMessageBox::question(this, "Сдача",
                              QString("Сдать партию (%1)?\nЭто зачтётся как поражение.").arg(who))
        != QMessageBox::Yes) {
        if (wasRunning && _clockTimer) {
            _clockTimer->start();
        }
        return;
    }

    if (_club) {
        _club->resignMatch(_matchId, whiteResigns);
    }
    clearSelection();
    refresh();
}

void GameDialog::drawAgree()
{
    const bool wasRunning = _clockTimer && _clockTimer->isActive();
    if (wasRunning) {
        _clockTimer->stop();
    }

    if (QMessageBox::question(this, "Ничья", "Зафиксировать ничью по согласию?") != QMessageBox::Yes) {
        if (wasRunning && _clockTimer) {
            _clockTimer->start();
        }
        return;
    }

    if (_club) {
        _club->agreeDraw(_matchId);
    }
    clearSelection();
    refresh();
}

void GameDialog::toStart()
{
    stopAutoPlay();
    _ply = 0;
    refresh();
}

void GameDialog::stepBack()
{
    stopAutoPlay();
    if (_ply > 0) {
        --_ply;
        refresh();
    }
}

void GameDialog::stepForward()
{
    stopAutoPlay();
    if (_game && _ply < static_cast<int>(_game->history().size())) {
        ++_ply;
        refresh();
    }
}

void GameDialog::toEnd()
{
    stopAutoPlay();
    if (_game) {
        _ply = static_cast<int>(_game->history().size());
        refresh();
    }
}

void GameDialog::onMoveActivated(int row)
{
    if (_editable || !_game || row < 0) {
        return;
    }

    stopAutoPlay();
    _ply = row + 1;
    refresh();
}

void GameDialog::setWhiteBottomMode()
{
    _orientationMode = OrientationMode::WhiteBottom;
    syncBoardOrientation();
}

void GameDialog::setBlackBottomMode()
{
    _orientationMode = OrientationMode::BlackBottom;
    syncBoardOrientation();
}

void GameDialog::setAutoFlipMode()
{
    _orientationMode = OrientationMode::AutoFlip;
    syncBoardOrientation();
}