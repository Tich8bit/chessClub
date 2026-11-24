#include "chessboard.h"

#include <algorithm>
#include <cctype>
#include <cstdlib>

namespace {

bool inside(int file, int rank)
{
    return file >= 0 && file < 8 && rank >= 0 && rank < 8;
}

int toSq(int file, int rank)
{
    return rank * 8 + file;
}

int sqFile(int sq)
{
    return sq % 8;
}

int sqRank(int sq)
{
    return sq / 8;
}

PieceColor oppositeColor(PieceColor c)
{
    if (c == PieceColor::White) return PieceColor::Black;
    if (c == PieceColor::Black) return PieceColor::White;
    return PieceColor::None;
}

} // namespace

ChessBoard::ChessBoard()
{
    reset();
}

void ChessBoard::reset()
{
    _squares.fill({});

    const PieceType backRank[8] = {
        PieceType::Rook, PieceType::Knight, PieceType::Bishop, PieceType::Queen,
        PieceType::King, PieceType::Bishop, PieceType::Knight, PieceType::Rook
    };

    for (int f = 0; f < 8; ++f) {
        _squares[toSq(f, 0)] = {PieceColor::White, backRank[f]};
        _squares[toSq(f, 1)] = {PieceColor::White, PieceType::Pawn};
        _squares[toSq(f, 6)] = {PieceColor::Black, PieceType::Pawn};
        _squares[toSq(f, 7)] = {PieceColor::Black, backRank[f]};
    }

    _whiteTurn = true;
    _wCastleK = true;
    _wCastleQ = true;
    _bCastleK = true;
    _bCastleQ = true;
    _enPassantSquare = -1;
    _history.clear();
}

ChessPiece ChessBoard::pieceAt(int square) const
{
    if (square < 0 || square >= 64) {
        return {};
    }
    return _squares[square];
}

bool ChessBoard::whiteToMove() const
{
    return _whiteTurn;
}

int ChessBoard::kingSquare(bool white) const
{
    const PieceColor target = white ? PieceColor::White : PieceColor::Black;
    for (int sq = 0; sq < 64; ++sq) {
        if (_squares[sq].color == target && _squares[sq].type == PieceType::King) {
            return sq;
        }
    }
    return -1;
}

bool ChessBoard::isSquareAttacked(int square, PieceColor byColor) const
{
    if (square < 0 || square >= 64 || byColor == PieceColor::None) {
        return false;
    }

    const int f = sqFile(square);
    const int r = sqRank(square);

    const int pawnDir = (byColor == PieceColor::White) ? -1 : 1;
    for (int df : {-1, 1}) {
        const int af = f + df;
        const int ar = r + pawnDir;
        if (inside(af, ar)) {
            const ChessPiece p = _squares[toSq(af, ar)];
            if (p.color == byColor && p.type == PieceType::Pawn) {
                return true;
            }
        }
    }

    const int knightDf[8] = {-2, -2, -1, -1, 1, 1, 2, 2};
    const int knightDr[8] = {-1, 1, -2, 2, -2, 2, -1, 1};
    for (int i = 0; i < 8; ++i) {
        const int af = f + knightDf[i];
        const int ar = r + knightDr[i];
        if (inside(af, ar)) {
            const ChessPiece p = _squares[toSq(af, ar)];
            if (p.color == byColor && p.type == PieceType::Knight) {
                return true;
            }
        }
    }

    const int kingDf[8] = {-1, -1, -1, 0, 0, 1, 1, 1};
    const int kingDr[8] = {-1, 0, 1, -1, 1, -1, 0, 1};
    for (int i = 0; i < 8; ++i) {
        const int af = f + kingDf[i];
        const int ar = r + kingDr[i];
        if (inside(af, ar)) {
            const ChessPiece p = _squares[toSq(af, ar)];
            if (p.color == byColor && p.type == PieceType::King) {
                return true;
            }
        }
    }

    const int diagDf[4] = {-1, -1, 1, 1};
    const int diagDr[4] = {-1, 1, -1, 1};
    for (int d = 0; d < 4; ++d) {
        int af = f + diagDf[d];
        int ar = r + diagDr[d];
        while (inside(af, ar)) {
            const ChessPiece p = _squares[toSq(af, ar)];
            if (!p.isEmpty()) {
                if (p.color == byColor && (p.type == PieceType::Bishop || p.type == PieceType::Queen)) {
                    return true;
                }
                break;
            }
            af += diagDf[d];
            ar += diagDr[d];
        }
    }

    const int orthoDf[4] = {-1, 1, 0, 0};
    const int orthoDr[4] = {0, 0, -1, 1};
    for (int d = 0; d < 4; ++d) {
        int af = f + orthoDf[d];
        int ar = r + orthoDr[d];
        while (inside(af, ar)) {
            const ChessPiece p = _squares[toSq(af, ar)];
            if (!p.isEmpty()) {
                if (p.color == byColor && (p.type == PieceType::Rook || p.type == PieceType::Queen)) {
                    return true;
                }
                break;
            }
            af += orthoDf[d];
            ar += orthoDr[d];
        }
    }

    return false;
}

bool ChessBoard::inCheck(bool white) const
{
    const int kSq = kingSquare(white);
    if (kSq < 0) {
        return false;
    }
    return isSquareAttacked(kSq, white ? PieceColor::Black : PieceColor::White);
}

void ChessBoard::generatePawnMoves(int sq, std::vector<ChessMove>& out) const
{
    const ChessPiece pawn = _squares[sq];
    const bool white = (pawn.color == PieceColor::White);
    const int dir = white ? 1 : -1;
    const int startRank = white ? 1 : 6;
    const int promoRank = white ? 7 : 0;

    const int f = sqFile(sq);
    const int r = sqRank(sq);

    auto addPawnMove = [&](int to, ChessPiece captured, bool ep = false) {
        if (sqRank(to) == promoRank) {
            for (PieceType pt : {PieceType::Queen, PieceType::Rook, PieceType::Bishop, PieceType::Knight}) {
                ChessMove m;
                m.from = sq;
                m.to = to;
                m.promotion = pt;
                m.captured = captured;
                m.isEnPassant = ep;
                out.push_back(m);
            }
        } else {
            ChessMove m;
            m.from = sq;
            m.to = to;
            m.captured = captured;
            m.isEnPassant = ep;
            out.push_back(m);
        }
    };

    const int oneRank = r + dir;
    if (inside(f, oneRank)) {
        const int oneSq = toSq(f, oneRank);
        if (_squares[oneSq].isEmpty()) {
            addPawnMove(oneSq, {});

            const int twoRank = r + 2 * dir;
            if (r == startRank && inside(f, twoRank)) {
                const int twoSq = toSq(f, twoRank);
                if (_squares[twoSq].isEmpty()) {
                    addPawnMove(twoSq, {});
                }
            }
        }
    }

    for (int df : {-1, 1}) {
        const int cf = f + df;
        if (!inside(cf, oneRank)) {
            continue;
        }
        const int capSq = toSq(cf, oneRank);
        const ChessPiece target = _squares[capSq];
        if (!target.isEmpty() && target.color != pawn.color) {
            addPawnMove(capSq, target);
        } else if (capSq == _enPassantSquare) {
            const int epCapSq = toSq(cf, r);
            addPawnMove(capSq, _squares[epCapSq], true);
        }
    }
}

void ChessBoard::generateKnightMoves(int sq, std::vector<ChessMove>& out) const
{
    const ChessPiece knight = _squares[sq];
    const int f = sqFile(sq);
    const int r = sqRank(sq);

    const int df[8] = {-2, -2, -1, -1, 1, 1, 2, 2};
    const int dr[8] = {-1, 1, -2, 2, -2, 2, -1, 1};

    for (int i = 0; i < 8; ++i) {
        const int nf = f + df[i];
        const int nr = r + dr[i];
        if (!inside(nf, nr)) {
            continue;
        }
        const int to = toSq(nf, nr);
        const ChessPiece target = _squares[to];
        if (target.isEmpty() || target.color != knight.color) {
            ChessMove m;
            m.from = sq;
            m.to = to;
            m.captured = target;
            out.push_back(m);
        }
    }
}

void ChessBoard::generateSlidingMoves(int sq, const int* dFile, const int* dRank, int dirs, std::vector<ChessMove>& out) const
{
    const ChessPiece piece = _squares[sq];
    const int f = sqFile(sq);
    const int r = sqRank(sq);

    for (int d = 0; d < dirs; ++d) {
        int nf = f + dFile[d];
        int nr = r + dRank[d];
        while (inside(nf, nr)) {
            const int to = toSq(nf, nr);
            const ChessPiece target = _squares[to];
            if (target.isEmpty()) {
                ChessMove m;
                m.from = sq;
                m.to = to;
                out.push_back(m);
            } else {
                if (target.color != piece.color) {
                    ChessMove m;
                    m.from = sq;
                    m.to = to;
                    m.captured = target;
                    out.push_back(m);
                }
                break;
            }
            nf += dFile[d];
            nr += dRank[d];
        }
    }
}

void ChessBoard::generateKingMoves(int sq, std::vector<ChessMove>& out) const
{
    const ChessPiece king = _squares[sq];
    const int f = sqFile(sq);
    const int r = sqRank(sq);

    const int df[8] = {-1, -1, -1, 0, 0, 1, 1, 1};
    const int dr[8] = {-1, 0, 1, -1, 1, -1, 0, 1};

    for (int i = 0; i < 8; ++i) {
        const int nf = f + df[i];
        const int nr = r + dr[i];
        if (!inside(nf, nr)) {
            continue;
        }
        const int to = toSq(nf, nr);
        const ChessPiece target = _squares[to];
        if (target.isEmpty() || target.color != king.color) {
            ChessMove m;
            m.from = sq;
            m.to = to;
            m.captured = target;
            out.push_back(m);
        }
    }

    const PieceColor enemy = oppositeColor(king.color);
    if (king.color == PieceColor::White && sq == toSq(4, 0) && !isSquareAttacked(toSq(4, 0), enemy)) {
        if (_wCastleK
            && _squares[toSq(5, 0)].isEmpty()
            && _squares[toSq(6, 0)].isEmpty()
            && _squares[toSq(7, 0)].color == PieceColor::White
            && _squares[toSq(7, 0)].type == PieceType::Rook
            && !isSquareAttacked(toSq(5, 0), enemy)
            && !isSquareAttacked(toSq(6, 0), enemy)) {
            ChessMove m;
            m.from = sq;
            m.to = toSq(6, 0);
            m.isCastling = true;
            out.push_back(m);
        }
        if (_wCastleQ
            && _squares[toSq(3, 0)].isEmpty()
            && _squares[toSq(2, 0)].isEmpty()
            && _squares[toSq(1, 0)].isEmpty()
            && _squares[toSq(0, 0)].color == PieceColor::White
            && _squares[toSq(0, 0)].type == PieceType::Rook
            && !isSquareAttacked(toSq(3, 0), enemy)
            && !isSquareAttacked(toSq(2, 0), enemy)) {
            ChessMove m;
            m.from = sq;
            m.to = toSq(2, 0);
            m.isCastling = true;
            out.push_back(m);
        }
    } else if (king.color == PieceColor::Black && sq == toSq(4, 7) && !isSquareAttacked(toSq(4, 7), enemy)) {
        if (_bCastleK
            && _squares[toSq(5, 7)].isEmpty()
            && _squares[toSq(6, 7)].isEmpty()
            && _squares[toSq(7, 7)].color == PieceColor::Black
            && _squares[toSq(7, 7)].type == PieceType::Rook
            && !isSquareAttacked(toSq(5, 7), enemy)
            && !isSquareAttacked(toSq(6, 7), enemy)) {
            ChessMove m;
            m.from = sq;
            m.to = toSq(6, 7);
            m.isCastling = true;
            out.push_back(m);
        }
        if (_bCastleQ
            && _squares[toSq(3, 7)].isEmpty()
            && _squares[toSq(2, 7)].isEmpty()
            && _squares[toSq(1, 7)].isEmpty()
            && _squares[toSq(0, 7)].color == PieceColor::Black
            && _squares[toSq(0, 7)].type == PieceType::Rook
            && !isSquareAttacked(toSq(3, 7), enemy)
            && !isSquareAttacked(toSq(2, 7), enemy)) {
            ChessMove m;
            m.from = sq;
            m.to = toSq(2, 7);
            m.isCastling = true;
            out.push_back(m);
        }
    }
}

std::vector<ChessMove> ChessBoard::pseudoLegalMoves() const
{
    std::vector<ChessMove> moves;
    const PieceColor turnColor = _whiteTurn ? PieceColor::White : PieceColor::Black;

    const int diagDf[4] = {-1, -1, 1, 1};
    const int diagDr[4] = {-1, 1, -1, 1};
    const int orthoDf[4] = {-1, 1, 0, 0};
    const int orthoDr[4] = {0, 0, -1, 1};
    const int queenDf[8] = {-1, -1, -1, 0, 0, 1, 1, 1};
    const int queenDr[8] = {-1, 0, 1, -1, 1, -1, 0, 1};

    for (int sq = 0; sq < 64; ++sq) {
        const ChessPiece p = _squares[sq];
        if (p.isEmpty() || p.color != turnColor) {
            continue;
        }

        switch (p.type) {
        case PieceType::Pawn:
            generatePawnMoves(sq, moves);
            break;
        case PieceType::Knight:
            generateKnightMoves(sq, moves);
            break;
        case PieceType::Bishop:
            generateSlidingMoves(sq, diagDf, diagDr, 4, moves);
            break;
        case PieceType::Rook:
            generateSlidingMoves(sq, orthoDf, orthoDr, 4, moves);
            break;
        case PieceType::Queen:
            generateSlidingMoves(sq, queenDf, queenDr, 8, moves);
            break;
        case PieceType::King:
            generateKingMoves(sq, moves);
            break;
        case PieceType::None:
            break;
        }
    }

    return moves;
}

void ChessBoard::applyMoveRaw(const ChessMove& move)
{
    ChessPiece moving = _squares[move.from];
    _squares[move.from] = {};

    if (move.isEnPassant) {
        const int capSq = toSq(sqFile(move.to), sqRank(move.from));
        _squares[capSq] = {};
    }

    if (move.isCastling) {
        if (move.to == toSq(6, 0)) {
            _squares[toSq(5, 0)] = _squares[toSq(7, 0)];
            _squares[toSq(7, 0)] = {};
        } else if (move.to == toSq(2, 0)) {
            _squares[toSq(3, 0)] = _squares[toSq(0, 0)];
            _squares[toSq(0, 0)] = {};
        } else if (move.to == toSq(6, 7)) {
            _squares[toSq(5, 7)] = _squares[toSq(7, 7)];
            _squares[toSq(7, 7)] = {};
        } else if (move.to == toSq(2, 7)) {
            _squares[toSq(3, 7)] = _squares[toSq(0, 7)];
            _squares[toSq(0, 7)] = {};
        }
    }

    if (moving.type == PieceType::Pawn && move.promotion != PieceType::None) {
        moving.type = move.promotion;
    }

    _squares[move.to] = moving;
}

bool ChessBoard::makeMove(const ChessMove& move)
{
    const auto legal = legalMoves();
    const auto it = std::find_if(legal.begin(), legal.end(), [&](const ChessMove& candidate) {
        return candidate.from == move.from
            && candidate.to == move.to
            && candidate.promotion == move.promotion;
    });

    if (it == legal.end()) {
        return false;
    }

    const ChessMove fullMove = *it;
    UndoRecord rec{
        fullMove,
        _squares[fullMove.from],
        _whiteTurn,
        _wCastleK,
        _wCastleQ,
        _bCastleK,
        _bCastleQ,
        _enPassantSquare
    };
    _history.push_back(rec);

    applyMoveRaw(fullMove);

    _enPassantSquare = -1;
    if (rec.movedPiece.type == PieceType::Pawn && std::abs(sqRank(fullMove.to) - sqRank(fullMove.from)) == 2) {
        const int midRank = (sqRank(fullMove.from) + sqRank(fullMove.to)) / 2;
        _enPassantSquare = toSq(sqFile(fullMove.from), midRank);
    }

    if (rec.movedPiece.type == PieceType::King) {
        if (rec.movedPiece.color == PieceColor::White) {
            _wCastleK = false;
            _wCastleQ = false;
        } else {
            _bCastleK = false;
            _bCastleQ = false;
        }
    }
    if (fullMove.from == toSq(7, 0) || fullMove.to == toSq(7, 0)) _wCastleK = false;
    if (fullMove.from == toSq(0, 0) || fullMove.to == toSq(0, 0)) _wCastleQ = false;
    if (fullMove.from == toSq(7, 7) || fullMove.to == toSq(7, 7)) _bCastleK = false;
    if (fullMove.from == toSq(0, 7) || fullMove.to == toSq(0, 7)) _bCastleQ = false;

    _whiteTurn = !_whiteTurn;
    return true;
}

bool ChessBoard::undoMove()
{
    if (_history.empty()) {
        return false;
    }

    const UndoRecord rec = _history.back();
    _history.pop_back();

    _whiteTurn = rec.prevWhiteTurn;
    _wCastleK = rec.wCastleK;
    _wCastleQ = rec.wCastleQ;
    _bCastleK = rec.bCastleK;
    _bCastleQ = rec.bCastleQ;
    _enPassantSquare = rec.prevEnPassant;

    _squares[rec.move.from] = rec.movedPiece;
    _squares[rec.move.to] = rec.move.isEnPassant ? ChessPiece{} : rec.move.captured;

    if (rec.move.isEnPassant) {
        const int capSq = toSq(sqFile(rec.move.to), sqRank(rec.move.from));
        _squares[capSq] = rec.move.captured;
    }

    if (rec.move.isCastling) {
        if (rec.move.to == toSq(6, 0)) {
            _squares[toSq(7, 0)] = _squares[toSq(5, 0)];
            _squares[toSq(5, 0)] = {};
        } else if (rec.move.to == toSq(2, 0)) {
            _squares[toSq(0, 0)] = _squares[toSq(3, 0)];
            _squares[toSq(3, 0)] = {};
        } else if (rec.move.to == toSq(6, 7)) {
            _squares[toSq(7, 7)] = _squares[toSq(5, 7)];
            _squares[toSq(5, 7)] = {};
        } else if (rec.move.to == toSq(2, 7)) {
            _squares[toSq(0, 7)] = _squares[toSq(3, 7)];
            _squares[toSq(3, 7)] = {};
        }
    }

    return true;
}

std::vector<ChessMove> ChessBoard::legalMoves() const
{
    std::vector<ChessMove> result;
    const auto pseudo = pseudoLegalMoves();
    const bool moverIsWhite = _whiteTurn;

    ChessBoard temp = *this;
    for (const auto& m : pseudo) {
        temp = *this;
        temp.applyMoveRaw(m);
        if (!temp.inCheck(moverIsWhite)) {
            result.push_back(m);
        }
    }

    return result;
}

ChessBoard::PositionState ChessBoard::evaluateState() const
{
    const auto moves = legalMoves();
    if (moves.empty()) {
        return inCheck(_whiteTurn) ? PositionState::Checkmate : PositionState::Stalemate;
    }

    int minorOrMajorCount = 0;
    int bishopCount = 0;
    int knightCount = 0;

    for (int sq = 0; sq < 64; ++sq) {
        const ChessPiece p = _squares[sq];
        if (p.isEmpty() || p.type == PieceType::King) {
            continue;
        }
        if (p.type == PieceType::Pawn || p.type == PieceType::Rook || p.type == PieceType::Queen) {
            ++minorOrMajorCount;
        } else if (p.type == PieceType::Bishop) {
            ++bishopCount;
        } else if (p.type == PieceType::Knight) {
            ++knightCount;
        }
    }

    if (minorOrMajorCount == 0 && bishopCount + knightCount <= 1) {
        return PositionState::InsufficientMaterial;
    }

    return PositionState::Normal;
}

std::string ChessBoard::toFen() const
{
    std::string fen;
    for (int r = 7; r >= 0; --r) {
        int empty = 0;
        for (int f = 0; f < 8; ++f) {
            const char c = _squares[toSq(f, r)].toChar();
            if (c == '.') {
                ++empty;
            } else {
                if (empty > 0) {
                    fen += std::to_string(empty);
                    empty = 0;
                }
                fen.push_back(c);
            }
        }
        if (empty > 0) {
            fen += std::to_string(empty);
        }
        if (r > 0) {
            fen.push_back('/');
        }
    }

    fen.push_back(' ');
    fen.push_back(_whiteTurn ? 'w' : 'b');
    return fen;
}