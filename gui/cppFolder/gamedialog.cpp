#include "gamedialog.h"

#include <QAction>
#include <QActionGroup>
#include <QHBoxLayout>
#include <QInputDialog>
#include <QLabel>
#include <QListWidget>
#include <QMenu>
#include <QMenuBar>
#include <QMessageBox>
#include <QPushButton>
#include <QSignalBlocker>
#include <QVBoxLayout>

#include <algorithm>

#include "chessClub.h"
#include "chessboardwidget.h"
#include "chessgame.h"
#include "exceptions/exception.h"
#include "match.h"

namespace {

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

} // namespace

GameDialog::GameDialog(ChessClub *club, int matchId, QWidget *parent)
    : QDialog(parent)
    , _club(club)
    , _matchId(matchId)
{
    auto match = _club->findMatchById(_matchId);
    _game = match ? match->game() : nullptr;
    _editable = match && _game && !match->getResult().isPlayed();
    _ply = _game ? static_cast<int>(_game->history().size()) : 0;

    setWindowTitle(QString("Партия #%1").arg(matchId));
    resize(1120, 780);

    // Палитра по референсу: пыльно-лавандовые акценты и мягкий серо-сиреневый графит.
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

    auto *menuBar = new QMenuBar(this);
    auto *viewMenu = menuBar->addMenu("Вид");
    auto *group = new QActionGroup(this);
    group->setExclusive(true);

    _whiteAction = viewMenu->addAction("Белые снизу");
    _whiteAction->setCheckable(true);
    _whiteAction->setChecked(true);
    group->addAction(_whiteAction);

    _blackAction = viewMenu->addAction("Чёрные снизу");
    _blackAction->setCheckable(true);
    group->addAction(_blackAction);

    connect(_whiteAction, &QAction::triggered, this, &GameDialog::orientWhiteBottom);
    connect(_blackAction, &QAction::triggered, this, &GameDialog::orientBlackBottom);

    _board = new ChessBoardWidget(this);
    _moveList = new QListWidget(this);
    _moveList->setMinimumWidth(250);

    _status = new QLabel(this);
    _status->setWordWrap(true);

    _undoButton = new QPushButton("Отменить ход", this);
    _resignButton = new QPushButton("Сдаться", this);
    _drawButton = new QPushButton("Ничья", this);
    auto *closeButton = new QPushButton("Закрыть", this);

    _startButton = new QPushButton("|<", this);
    _prevButton = new QPushButton("<", this);
    _nextButton = new QPushButton(">", this);
    _endButton = new QPushButton(">|", this);

    auto *navLayout = new QHBoxLayout;
    navLayout->setSpacing(8);
    navLayout->addWidget(_startButton);
    navLayout->addWidget(_prevButton);
    navLayout->addWidget(_nextButton);
    navLayout->addWidget(_endButton);

    auto *right = new QVBoxLayout;
    right->setSpacing(10);
    right->addWidget(new QLabel("Ходы:", this));
    right->addWidget(_moveList, 1);
    right->addLayout(navLayout);
    right->addWidget(_status);
    right->addWidget(_undoButton);
    right->addWidget(_resignButton);
    right->addWidget(_drawButton);
    right->addWidget(closeButton);

    auto *content = new QHBoxLayout;
    content->setSpacing(16);
    content->addWidget(_board, 3);
    content->addLayout(right, 1);

    auto *root = new QVBoxLayout(this);
    root->setMenuBar(menuBar);
    root->addLayout(content);

    connect(_board, &ChessBoardWidget::squareClicked, this, &GameDialog::onSquareClicked);
    connect(_undoButton, &QPushButton::clicked, this, &GameDialog::undoMove);
    connect(_resignButton, &QPushButton::clicked, this, &GameDialog::resign);
    connect(_drawButton, &QPushButton::clicked, this, &GameDialog::drawAgree);
    connect(closeButton, &QPushButton::clicked, this, &GameDialog::accept);

    connect(_startButton, &QPushButton::clicked, this, &GameDialog::toStart);
    connect(_prevButton, &QPushButton::clicked, this, &GameDialog::stepBack);
    connect(_nextButton, &QPushButton::clicked, this, &GameDialog::stepForward);
    connect(_endButton, &QPushButton::clicked, this, &GameDialog::toEnd);
    connect(_moveList, &QListWidget::currentRowChanged, this, &GameDialog::onMoveActivated);

    refresh();
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
    if (_autoFlip && _editable && _game && !_game->isOver()) {
        _flipped = !_game->whiteToMove();
    }

    if (_whiteAction && _blackAction) {
        QSignalBlocker b1(_whiteAction);
        QSignalBlocker b2(_blackAction);
        _whiteAction->setChecked(!_flipped);
        _blackAction->setChecked(_flipped);
    }

    _board->setFlipped(_flipped);
}

void GameDialog::refresh()
{
    if (!_game) {
        _status->setText("У этой партии нет истории ходов.");
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
        const auto match = _club->findMatchById(_matchId);
        QString result;
        if (match && match->getResult().isPlayed()) {
            result = QString(" · результат: %1").arg(QString::fromStdString(match->getResult().toString()));
        }
        _status->setText(QString("Просмотр: ход %1 из %2%3").arg(_ply).arg(history.size()).arg(result));
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

    updateNavigation();
}

void GameDialog::updateNavigation()
{
    if (!_game) {
        return;
    }

    const int total = static_cast<int>(_game->history().size());
    _startButton->setEnabled(_ply > 0);
    _prevButton->setEnabled(_ply > 0);
    _nextButton->setEnabled(_ply < total);
    _endButton->setEnabled(_ply < total);

    const bool reviewButtons = !_editable;
    _startButton->setVisible(reviewButtons);
    _prevButton->setVisible(reviewButtons);
    _nextButton->setVisible(reviewButtons);
    _endButton->setVisible(reviewButtons);
}

void GameDialog::clearSelection()
{
    _selected = -1;
    _targets.clear();
}

QString GameDialog::askPromotion() const
{
    const QStringList options{"Ферзь", "Ладья", "Слон", "Конь"};
    bool ok = false;
    const QString choice = QInputDialog::getItem(const_cast<GameDialog *>(this), "Превращение",
                                                 "Выберите фигуру:", options, 0, false, &ok);
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
                _club->playMove(_matchId, uci.toStdString());
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
    if (_club->undoMove(_matchId)) {
        clearSelection();
        refresh();
    }
}

void GameDialog::resign()
{
    const bool whiteResigns = _game->whiteToMove();
    const QString who = whiteResigns ? "белые" : "чёрные";

    if (QMessageBox::question(this, "Сдача",
                              QString("Сдать партию (%1)?\nЭто зачтётся как поражение.").arg(who))
        != QMessageBox::Yes) {
        return;
    }

    _club->resignMatch(_matchId, whiteResigns);
    clearSelection();
    refresh();
}

void GameDialog::drawAgree()
{
    if (QMessageBox::question(this, "Ничья", "Зафиксировать ничью по согласию?") != QMessageBox::Yes) {
        return;
    }

    _club->agreeDraw(_matchId);
    clearSelection();
    refresh();
}

void GameDialog::toStart()
{
    _ply = 0;
    refresh();
}

void GameDialog::stepBack()
{
    if (_ply > 0) {
        --_ply;
        refresh();
    }
}

void GameDialog::stepForward()
{
    if (_game && _ply < static_cast<int>(_game->history().size())) {
        ++_ply;
        refresh();
    }
}

void GameDialog::toEnd()
{
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

    _ply = row + 1;
    refresh();
}

void GameDialog::orientWhiteBottom()
{
    _autoFlip = false;
    _flipped = false;
    syncBoardOrientation();
}

void GameDialog::orientBlackBottom()
{
    _autoFlip = false;
    _flipped = true;
    syncBoardOrientation();
}