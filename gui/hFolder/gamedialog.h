#pragma once

#include <QDialog>
#include <QSet>

#include <memory>

class ChessClub;
class ChessBoardWidget;
class ChessGame;
class QLabel;
class QListWidget;
class QPushButton;

// Окно партии: доска, список ходов, управление.
class GameDialog : public QDialog
{
    Q_OBJECT

public:
    GameDialog(ChessClub *club, int matchId, QWidget *parent = nullptr);

private slots:
    void onSquareClicked(int square);
    void undoMove();
    void resign();
    void drawAgree();

private:
    void refresh();
    void clearSelection();
    QString askPromotion() const;

    ChessClub *_club = nullptr;
    int _matchId = -1;
    std::shared_ptr<ChessGame> _game;

    ChessBoardWidget *_board = nullptr;
    QListWidget *_moveList = nullptr;
    QLabel *_status = nullptr;
    QPushButton *_undoButton = nullptr;
    QPushButton *_resignButton = nullptr;
    QPushButton *_drawButton = nullptr;

    int _selected = -1;
    QSet<int> _targets;
};
