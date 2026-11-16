#include "gamedialog.h"

#include <QHBoxLayout>
#include <QInputDialog>
#include <QLabel>
#include <QListWidget>
#include <QMessageBox>
#include <QPushButton>
#include <QVBoxLayout>

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

    setWindowTitle(QString("Партия #%1").arg(matchId));
    resize(980, 700);

    _board = new ChessBoardWidget(this);
    _board->setGame(_game.get());

    _moveList = new QListWidget(this);
    _moveList->setMinimumWidth(220);

    _status = new QLabel(this);
    _status->setWordWrap(true);

    _undoButton = new QPushButton("Отменить ход", this);
    _resignButton = new QPushButton("Сдаться", this);
    _drawButton = new QPushButton("Ничья", this);
    auto *closeButton = new QPushButton("Закрыть", this);

    auto *right = new QVBoxLayout;
    right->addWidget(new QLabel("Ходы:", this));
    right->addWidget(_moveList, 1);
    right->addWidget(_status);
    right->addWidget(_undoButton);
    right->addWidget(_resignButton);
    right->addWidget(_drawButton);
    right->addWidget(closeButton);

    auto *layout = new QHBoxLayout(this);
    layout->addWidget(_board, 3);
    layout->addLayout(right, 1);

    connect(_board, &ChessBoardWidget::squareClicked, this, &GameDialog::onSquareClicked);
    connect(_undoButton, &QPushButton::clicked, this, &GameDialog::undoMove);
    connect(_resignButton, &QPushButton::clicked, this, &GameDialog::resign);
    connect(_drawButton, &QPushButton::clicked, this, &GameDialog::drawAgree);
    connect(closeButton, &QPushButton::clicked, this, &QDialog::accept);

    refresh();
}

void GameDialog::refresh()
{
    if (!_game) {
        _status->setText("У этой партии нет истории ходов.");
        return;
    }

    _board->setGame(_game.get());
    _board->setSelectedSquare(_selected);
    _board->setLegalTargets(_targets);

    const auto &history = _game->history();
    _board->setLastMove(history.empty() ? -1 : squareIndex(QString::fromStdString(history.back()).left(2)),
                        history.empty() ? -1 : squareIndex(QString::fromStdString(history.back()).mid(2, 2)));

    _board->setCheckSquare(_game->inCheck() ? _game->kingSquare(_game->whiteToMove()) : -1);

    _moveList->clear();
    for (size_t i = 0; i < history.size(); i += 2) {
        QString line = QString("%1. %2").arg(i / 2 + 1).arg(QString::fromStdString(history[i]));
        if (i + 1 < history.size()) {
            line += "  " + QString::fromStdString(history[i + 1]);
        }
        _moveList->addItem(line);
    }
    _moveList->scrollToBottom();

    QString text;
    if (_game->isOver()) {
        if (_game->whiteWon()) {
            text = "Партия окончена: победа белых.";
        } else if (_game->blackWon()) {
            text = "Партия окончена: победа чёрных.";
        } else {
            text = "Партия окончена: ничья.";
        }
    } else {
        text = _game->whiteToMove() ? "Ход белых." : "Ход чёрных.";
        if (_game->inCheck()) {
            text += " Шах!";
        }
    }
    _status->setText(text);

    _undoButton->setEnabled(!_game->isOver() && !history.empty());
    _resignButton->setEnabled(!_game->isOver());
    _drawButton->setEnabled(!_game->isOver());
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
    if (!_game || _game->isOver()) {
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

    if (QMessageBox::question(this, "Сдача", QString("Сдать партию (%1)?\nЭто зачтётся как поражение.").arg(who))
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
