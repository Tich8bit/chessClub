#pragma once

enum class PieceColor { None, White, Black };

enum class PieceType { None, Pawn, Knight, Bishop, Rook, Queen, King };

struct ChessPiece {
    PieceColor color = PieceColor::None;
    PieceType type = PieceType::None;

    bool isEmpty() const;
    char toChar() const;
    static ChessPiece fromChar(char c);
};