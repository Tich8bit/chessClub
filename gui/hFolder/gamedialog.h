#pragma once

#include <QDialog>
#include <QSet>

#include <memory>

#include "tournament.h"

class ChessClub;
class ChessBoardWidget;
class ChessGame;
class QAction;
class QLabel;
class QListWidget;
class QPushButton;
class QTimer;

// Окно партии: доска, список ходов, часы и управление.
// Если партия не завершена — можно ходить; если завершена — только просмотр.
class GameDialog : public QDialog
{
    Q_OBJECT

public:
    GameDialog(ChessClub *club,
               int matchId,
               Tournament::TimeControl timeControl = Tournament::TimeControl::Rapid,
               QWidget *parent = nullptr);
    ~GameDialog() override;

private slots:
    void onSquareClicked(int square);
    void undoMove();
    void resign();
    void drawAgree();
    void onClockTick();

    void toStart();
    void stepBack();
    void stepForward();
    void toEnd();

    void onMoveActivated(int row);
    void orientWhiteBottom();
    void orientBlackBottom();

private:
    static int initialSecondsForControl(Tournament::TimeControl control);
    static QString formatSeconds(int seconds);

    void rebuildView();
    void syncBoardOrientation();
    void refresh();
    void updateClocks();
    void syncClockTimer();
    void clearSelection();
    QString askPromotion() const;
    void updateNavigation();

    ChessClub *_club = nullptr;
    int _matchId = -1;
    Tournament::TimeControl _timeControl = Tournament::TimeControl::Rapid;
    bool _editable = false;
    bool _flipped = false;
    bool _autoFlip = true;
    int _ply = 0;

    int _whiteSecondsLeft = 600;
    int _blackSecondsLeft = 600;
    bool _flagged = false;
    QTimer *_clockTimer = nullptr;

    std::shared_ptr<ChessGame> _game;      // настоящая партия
    std::shared_ptr<ChessGame> _viewGame;  // позиция для показа

    QAction *_whiteAction = nullptr;
    QAction *_blackAction = nullptr;

    ChessBoardWidget *_board = nullptr;
    QListWidget *_moveList = nullptr;
    QLabel *_whiteClockLabel = nullptr;
    QLabel *_blackClockLabel = nullptr;
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