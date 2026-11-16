#include "chessboardwidget.h"

#include <QFont>
#include <QMouseEvent>
#include <QPainter>
#include <QPainterPath>
#include <QTransform>

#include <algorithm>

#include "chessgame.h"

namespace {

const QColor LIGHT_SQUARE(240, 217, 181);
const QColor DARK_SQUARE(181, 136, 99);
const QColor SELECTED(255, 235, 59, 140);
const QColor TARGET(60, 160, 90, 150);
const QColor LAST_MOVE(205, 210, 110, 110);
const QColor CHECK(220, 60, 60, 160);

QString glyphFor(char piece)
{
    const bool white = piece >= 'A' && piece <= 'Z';
    switch (piece >= 'a' ? piece : static_cast<char>(piece - 'A' + 'a')) {
    case 'k': return white ? QString::fromUtf8("\u2654") : QString::fromUtf8("\u265A");
    case 'q': return white ? QString::fromUtf8("\u2655") : QString::fromUtf8("\u265B");
    case 'r': return white ? QString::fromUtf8("\u2656") : QString::fromUtf8("\u265C");
    case 'b': return white ? QString::fromUtf8("\u2657") : QString::fromUtf8("\u265D");
    case 'n': return white ? QString::fromUtf8("\u2658") : QString::fromUtf8("\u265E");
    case 'p': return white ? QString::fromUtf8("\u2659") : QString::fromUtf8("\u265F");
    default:  return QString();
    }
}

} // namespace

ChessBoardWidget::ChessBoardWidget(QWidget *parent)
    : QWidget(parent)
{
    setMinimumSize(400, 400);
    setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Expanding);
}

void ChessBoardWidget::setGame(ChessGame *game)
{
    _game = game;
    update();
}

void ChessBoardWidget::setSelectedSquare(int square)
{
    _selected = square;
    update();
}

void ChessBoardWidget::setLegalTargets(const QSet<int> &squares)
{
    _targets = squares;
    update();
}

void ChessBoardWidget::setLastMove(int from, int to)
{
    _lastFrom = from;
    _lastTo = to;
    update();
}

void ChessBoardWidget::setCheckSquare(int square)
{
    _check = square;
    update();
}

void ChessBoardWidget::setFlipped(bool flipped)
{
    _flipped = flipped;
    update();
}

bool ChessBoardWidget::isFlipped() const
{
    return _flipped;
}

QSize ChessBoardWidget::sizeHint() const
{
    return QSize(640, 640);
}

int ChessBoardWidget::cellSize() const
{
    return std::max(1, std::min(width(), height()) / 8);
}

void ChessBoardWidget::displayCoords(int square, int &column, int &row) const
{
    const int file = square % 8;
    const int rank = square / 8;

    column = _flipped ? 7 - file : file;
    row = _flipped ? rank : 7 - rank;
}

QRect ChessBoardWidget::cellRect(int square) const
{
    const int cell = cellSize();
    const int offsetX = (width() - cell * 8) / 2;
    const int offsetY = (height() - cell * 8) / 2;

    int column = 0;
    int row = 0;
    displayCoords(square, column, row);

    return QRect(offsetX + column * cell, offsetY + row * cell, cell, cell);
}

int ChessBoardWidget::squareAt(const QPoint &point) const
{
    const int cell = cellSize();
    const int offsetX = (width() - cell * 8) / 2;
    const int offsetY = (height() - cell * 8) / 2;

    const int x = point.x() - offsetX;
    const int y = point.y() - offsetY;

    if (cell <= 0 || x < 0 || y < 0 || x >= cell * 8 || y >= cell * 8) {
        return -1;
    }

    const int column = x / cell;
    const int row = y / cell;

    const int file = _flipped ? 7 - column : column;
    const int rank = _flipped ? row : 7 - row;

    return rank * 8 + file;
}

void ChessBoardWidget::mousePressEvent(QMouseEvent *event)
{
    const int square = squareAt(event->pos());
    if (square >= 0) {
        emit squareClicked(square);
    }
}

void ChessBoardWidget::paintEvent(QPaintEvent *)
{
    QPainter painter(this);
    painter.setRenderHint(QPainter::Antialiasing, true);

    const int cell = cellSize();
    const int offsetX = (width() - cell * 8) / 2;
    const int offsetY = (height() - cell * 8) / 2;

    for (int square = 0; square < 64; ++square) {
        const QRect rect = cellRect(square);
        const int file = square % 8;
        const int rank = square / 8;
        const bool light = (file + rank) % 2 == 1;
        painter.fillRect(rect, light ? LIGHT_SQUARE : DARK_SQUARE);
    }

    if (_lastFrom >= 0) {
        painter.fillRect(cellRect(_lastFrom), LAST_MOVE);
    }
    if (_lastTo >= 0) {
        painter.fillRect(cellRect(_lastTo), LAST_MOVE);
    }
    if (_check >= 0) {
        painter.fillRect(cellRect(_check), CHECK);
    }
    if (_selected >= 0) {
        painter.fillRect(cellRect(_selected), SELECTED);
    }

    painter.setPen(Qt::NoPen);
    painter.setBrush(TARGET);
    for (int square : _targets) {
        const QRect rect = cellRect(square);
        painter.drawEllipse(rect.center(), cell / 7, cell / 7);
    }
    painter.setBrush(Qt::NoBrush);

    // Координаты — внутри крайних клеток, чтобы доска не съезжала.
    QFont coordFont = painter.font();
    coordFont.setPixelSize(std::max(9, cell / 4));
    coordFont.setBold(true);
    painter.setFont(coordFont);

    for (int column = 0; column < 8; ++column) {
        const QRect rect(offsetX + column * cell, offsetY + 7 * cell, cell, cell);
        const int file = _flipped ? 7 - column : column;
        const QColor color = (file + ( _flipped ? 7 : 0)) % 2 == 1 ? DARK_SQUARE : LIGHT_SQUARE;
        painter.setPen(color);
        painter.drawText(rect.adjusted(4, 0, -4, -2), Qt::AlignRight | Qt::AlignBottom,
                         QString(QChar('a' + file)));
    }

    for (int row = 0; row < 8; ++row) {
        const QRect rect(offsetX, offsetY + row * cell, cell, cell);
        const int rank = _flipped ? row : 7 - row;
        const QColor color = (rank + 0) % 2 == 1 ? LIGHT_SQUARE : DARK_SQUARE;
        painter.setPen(color);
        painter.drawText(rect.adjusted(4, 2, -4, 0), Qt::AlignLeft | Qt::AlignTop,
                         QString::number(rank + 1));
    }

    if (!_game) {
        return;
    }

    QFont pieceFont(QStringLiteral("Segoe UI Symbol"));
    pieceFont.setPixelSize(static_cast<int>(cell * 0.78));
    painter.setFont(pieceFont);

    for (int square = 0; square < 64; ++square) {
        const char piece = _game->pieceAt(square);
        if (piece == '.') {
            continue;
        }

        const QString glyph = glyphFor(piece);
        if (glyph.isEmpty()) {
            continue;
        }

        const bool white = piece >= 'A' && piece <= 'Z';
        const QRect rect = cellRect(square);

        // Центрируем глиф по его реальным границам, а не по метрикам шрифта:
        // иначе фигуры выглядят смещёнными внутри клетки.
        QPainterPath path;
        path.addText(0, 0, pieceFont, glyph);

        const QRectF bounds = path.boundingRect();
        const qreal maxSide = std::max(bounds.width(), bounds.height());
        const qreal scale = maxSide > 0.0 ? std::min<qreal>(1.0, (cell * 0.84) / maxSide) : 1.0;

        QTransform transform;
        transform.translate(rect.center().x(), rect.center().y());
        transform.scale(scale, scale);
        transform.translate(-bounds.center().x(), -bounds.center().y());

        painter.setPen(QPen(white ? QColor(35, 35, 35) : QColor(240, 240, 240), 1.4));
        painter.setBrush(white ? QColor(252, 252, 252) : QColor(28, 28, 28));
        painter.drawPath(transform.map(path));
    }
}
