#pragma once

#include <array>
#include <string>
#include <vector>

#include "chessmove.h"
#include "chesspiece.h"

class ChessBoard {
public:
    enum class PositionState {
        Normal,
        Checkmate,
        Stalemate,
        InsufficientMaterial
    };

    ChessBoard();

    void reset();
    ChessPiece pieceAt(int square) const;
    bool whiteToMove() const;
    int kingSquare(bool white) const;
    bool isSquareAttacked(int square, PieceColor byColor) const;
    bool inCheck(bool white) const;

    std::vector<ChessMove> pseudoLegalMoves() const;
    std::vector<ChessMove> legalMoves() const;

    bool makeMove(const ChessMove& move);
    bool undoMove();

    PositionState evaluateState() const;
    std::string toFen() const;

private:
    struct UndoRecord {
        ChessMove move;
        ChessPiece movedPiece;
        bool prevWhiteTurn;
        bool wCastleK;
        bool wCastleQ;
        bool bCastleK;
        bool bCastleQ;
        int prevEnPassant;
    };

    std::array<ChessPiece, 64> _squares{};
    bool _whiteTurn = true;
    bool _wCastleK = true;
    bool _wCastleQ = true;
    bool _bCastleK = true;
    bool _bCastleQ = true;
    int _enPassantSquare = -1;
    std::vector<UndoRecord> _history;

    void generatePawnMoves(int sq, std::vector<ChessMove>& out) const;
    void generateKnightMoves(int sq, std::vector<ChessMove>& out) const;
    void generateSlidingMoves(int sq, const int* dFile, const int* dRank, int dirs, std::vector<ChessMove>& out) const;
    void generateKingMoves(int sq, std::vector<ChessMove>& out) const;
    void applyMoveRaw(const ChessMove& move);
};