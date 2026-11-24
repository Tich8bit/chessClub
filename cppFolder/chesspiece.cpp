#include "chesspiece.h"

#include <cctype>

bool ChessPiece::isEmpty() const
{
    return color == PieceColor::None || type == PieceType::None;
}

char ChessPiece::toChar() const
{
    if (isEmpty()) {
        return '.';
    }

    char ch = '?';
    switch (type) {
    case PieceType::Pawn:   ch = 'p'; break;
    case PieceType::Knight: ch = 'n'; break;
    case PieceType::Bishop: ch = 'b'; break;
    case PieceType::Rook:   ch = 'r'; break;
    case PieceType::Queen:  ch = 'q'; break;
    case PieceType::King:   ch = 'k'; break;
    case PieceType::None:   return '.';
    }

    return color == PieceColor::White
               ? static_cast<char>(std::toupper(static_cast<unsigned char>(ch)))
               : ch;
}

ChessPiece ChessPiece::fromChar(char c)
{
    if (c == '.' || c == '\0') {
        return {};
    }

    const bool white = std::isupper(static_cast<unsigned char>(c)) != 0;
    const char lower = static_cast<char>(std::tolower(static_cast<unsigned char>(c)));

    PieceType t = PieceType::None;
    switch (lower) {
    case 'p': t = PieceType::Pawn; break;
    case 'n': t = PieceType::Knight; break;
    case 'b': t = PieceType::Bishop; break;
    case 'r': t = PieceType::Rook; break;
    case 'q': t = PieceType::Queen; break;
    case 'k': t = PieceType::King; break;
    default:  return {};
    }

    return {white ? PieceColor::White : PieceColor::Black, t};
}