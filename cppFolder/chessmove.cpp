#include "chessmove.h"

#include <cctype>

namespace {

char promoChar(PieceType p)
{
    switch (p) {
    case PieceType::Queen:  return 'q';
    case PieceType::Rook:   return 'r';
    case PieceType::Bishop: return 'b';
    case PieceType::Knight: return 'n';
    default:                return '\0';
    }
}

PieceType charToPromo(char c)
{
    switch (static_cast<char>(std::tolower(static_cast<unsigned char>(c)))) {
    case 'q': return PieceType::Queen;
    case 'r': return PieceType::Rook;
    case 'b': return PieceType::Bishop;
    case 'n': return PieceType::Knight;
    default:  return PieceType::None;
    }
}

} // namespace

std::string ChessMove::toUci() const
{
    if (from < 0 || from >= 64 || to < 0 || to >= 64) {
        return {};
    }

    std::string res;
    res.push_back(static_cast<char>('a' + (from % 8)));
    res.push_back(static_cast<char>('1' + (from / 8)));
    res.push_back(static_cast<char>('a' + (to % 8)));
    res.push_back(static_cast<char>('1' + (to / 8)));

    const char pr = promoChar(promotion);
    if (pr != '\0') {
        res.push_back(pr);
    }

    return res;
}

ChessMove ChessMove::fromUci(const std::string& uci)
{
    if (uci.size() < 4) {
        return {};
    }

    const int ff = uci[0] - 'a';
    const int fr = uci[1] - '1';
    const int tf = uci[2] - 'a';
    const int tr = uci[3] - '1';

    if (ff < 0 || ff > 7 || fr < 0 || fr > 7 || tf < 0 || tf > 7 || tr < 0 || tr > 7) {
        return {};
    }

    ChessMove move;
    move.from = fr * 8 + ff;
    move.to = tr * 8 + tf;
    if (uci.size() >= 5) {
        move.promotion = charToPromo(uci[4]);
    }
    return move;
}