#pragma once

#include <memory>
#include <string>
#include <vector>

// Обёртка над chess.hpp: хранит партию, историю ходов и статус.
// Правила (легальность, шах, мат, пат, ничьи) считает библиотека.
class ChessGame {
public:
    enum class Status { InProgress, Checkmate, Stalemate, Draw, Resigned };
    enum class EndReason {
        None,
        Checkmate,
        Stalemate,
        InsufficientMaterial,
        FiftyMoves,
        Repetition,
        Resignation,
        Agreement
    };

    ChessGame();
    ~ChessGame();

    ChessGame(const ChessGame&) = delete;
    ChessGame& operator=(const ChessGame&) = delete;

    // Ход в UCI-нотации: "e2e4", "e7e8q". false — ход нелегален или партия окончена.
    bool makeMove(const std::string& uci);
    bool undoMove();

    // Завершение по желанию игрока: сдача или согласованная ничья.
    void resign(bool whiteSide);
    void agreeDraw();

    bool isOver() const;
    Status status() const;
    EndReason endReason() const;
    bool whiteWon() const;
    bool blackWon() const;

    bool inCheck() const;
    bool whiteToMove() const;
    std::string fen() const;

    std::vector<std::string> legalMoves() const;
    const std::vector<std::string>& history() const;

    // Для интерфейса: фигура на клетке ('P','n','.' — пусто) и позиция короля.
    char pieceAt(int square) const;
    int kingSquare(bool white) const;

    // Восстановление партии из сохранённых ходов.
    bool replay(const std::vector<std::string>& moves);

private:
    struct Impl;
    std::unique_ptr<Impl> _impl;
};
