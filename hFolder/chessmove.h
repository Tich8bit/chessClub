#pragma once

#include <string>

#include "chesspiece.h"

struct ChessMove {
    int from = -1;
    int to = -1;
    PieceType promotion = PieceType::None;
    ChessPiece captured{};
    bool isEnPassant = false;
    bool isCastling = false;

    std::string toUci() const;
    static ChessMove fromUci(const std::string& uci);
};