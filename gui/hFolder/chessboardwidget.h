#pragma once

#include <QSet>
#include <QWidget>

class ChessGame;

// Виджет шахматной доски: рисует позицию и подсветку.
// Вся логика хода — в GameDialog, виджет только сообщает о клике.
class ChessBoardWidget : public QWidget
{
    Q_OBJECT

public:
    explicit ChessBoardWidget(QWidget *parent = nullptr);

    void setGame(ChessGame *game);
    void setSelectedSquare(int square);
    void setLegalTargets(const QSet<int> &squares);
    void setLastMove(int from, int to);
    void setCheckSquare(int square);

    // Ориентация: false — белые снизу, true — белые сверху.
    void setFlipped(bool flipped);
    bool isFlipped() const;

    QSize sizeHint() const override;

signals:
    void squareClicked(int square);

protected:
    void paintEvent(QPaintEvent *event) override;
    void mousePressEvent(QMouseEvent *event) override;

private:
    int squareAt(const QPoint &point) const;
    int cellSize() const;
    QRect cellRect(int square) const;
    void displayCoords(int square, int &column, int &row) const;

    ChessGame *_game = nullptr;
    int _selected = -1;
    int _lastFrom = -1;
    int _lastTo = -1;
    int _check = -1;
    bool _flipped = false;
    QSet<int> _targets;
};
