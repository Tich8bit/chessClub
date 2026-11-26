#pragma once

#include <QDialog>
#include <QSet>

#include <memory>

class ChessClub;
class ChessBoardWidget;
class ChessGame;
class QAction;
class QLabel;
class QListWidget;
class QPushButton;

// Окно партии: доска, список ходов, управление.
// Если партия не завершена — можно ходить; если завершена — только просмотр.
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

    void toStart();
    void stepBack();
    void stepForward();
    void toEnd();

    void onMoveActivated(int row);
    void orientWhiteBottom();
    void orientBlackBottom();

private:
    void rebuildView();
    void syncBoardOrientation();
    void refresh();
    void clearSelection();
    QString askPromotion() const;
    void updateNavigation();

    ChessClub *_club = nullptr;
    int _matchId = -1;
    bool _editable = false;
    bool _flipped = false;
    bool _autoFlip = true;
    int _ply = 0;

    std::shared_ptr<ChessGame> _game;      // настоящая партия
    std::shared_ptr<ChessGame> _viewGame;  // позиция для показа

    QAction *_whiteAction = nullptr;
    QAction *_blackAction = nullptr;

    ChessBoardWidget *_board = nullptr;
    QListWidget *_moveList = nullptr;
    QLabel *_status = nullptr;
    QPushButton *_undoButton = nullptr;
    QPushButton *_resignButton = nullptr;
    QPushButton *_drawButton = nullptr;
    QPushButton *_startButton = nullptr;
    QPushButton *_prevButton = nullptr;
    QPushButton *_nextButton = nullptr;
    QPushButton *_endButton = nullptr;

    int _selected = -1;
    QSet<int> _targets;
};