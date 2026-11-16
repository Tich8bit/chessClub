#include "chessgame.h"

#include <cctype>

#include "chess.hpp"

struct ChessGame::Impl {
    chess::Board board;
    std::vector<chess::Move> moves;
    std::vector<std::string> uciMoves;

    Status status = Status::InProgress;
    EndReason reason = EndReason::None;
    bool whiteWon = false;
    bool blackWon = false;
};

namespace {

ChessGame::EndReason mapReason(chess::GameResultReason reason)
{
    switch (reason) {
    case chess::GameResultReason::CHECKMATE:             return ChessGame::EndReason::Checkmate;
    case chess::GameResultReason::STALEMATE:             return ChessGame::EndReason::Stalemate;
    case chess::GameResultReason::INSUFFICIENT_MATERIAL: return ChessGame::EndReason::InsufficientMaterial;
    case chess::GameResultReason::FIFTY_MOVE_RULE:       return ChessGame::EndReason::FiftyMoves;
    case chess::GameResultReason::THREEFOLD_REPETITION:  return ChessGame::EndReason::Repetition;
    case chess::GameResultReason::NONE:                  break;
    }
    return ChessGame::EndReason::None;
}

} // namespace

ChessGame::ChessGame()
    : _impl(std::make_unique<Impl>())
{
}

ChessGame::~ChessGame() = default;

bool ChessGame::makeMove(const std::string& uci)
{
    if (isOver()) {
        return false;
    }

    try {
        const chess::Move move = chess::uci::uciToMove(_impl->board, uci);
        if (move == chess::Move::NO_MOVE) {
            return false;
        }
        if (!_impl->board.isLegal(move)) {
            return false;
        }

        _impl->board.makeMove(move);
        _impl->moves.push_back(move);
        _impl->uciMoves.push_back(chess::uci::moveToUci(move));

        const auto over = _impl->board.isGameOver();
        if (over.second == chess::GameResult::NONE) {
            _impl->status = Status::InProgress;
            _impl->reason = EndReason::None;
            return true;
        }

        _impl->reason = mapReason(over.first);

        if (over.first == chess::GameResultReason::CHECKMATE) {
            _impl->status = Status::Checkmate;
            // Ход сделан, значит под матом сторона, которая должна ходить.
            if (_impl->board.sideToMove() == chess::Color::WHITE) {
                _impl->blackWon = true;
            } else {
                _impl->whiteWon = true;
            }
        } else {
            _impl->status = Status::Draw;
        }

        return true;
    } catch (...) {
        return false;
    }
}

bool ChessGame::undoMove()
{
    if (_impl->moves.empty() || _impl->status == Status::Resigned) {
        return false;
    }

    _impl->board.unmakeMove(_impl->moves.back());
    _impl->moves.pop_back();
    _impl->uciMoves.pop_back();

    _impl->status = Status::InProgress;
    _impl->reason = EndReason::None;
    _impl->whiteWon = false;
    _impl->blackWon = false;

    return true;
}

void ChessGame::resign(bool whiteSide)
{
    if (isOver()) {
        return;
    }
    _impl->status = Status::Resigned;
    _impl->reason = EndReason::Resignation;
    _impl->whiteWon = !whiteSide;
    _impl->blackWon = whiteSide;
}

void ChessGame::agreeDraw()
{
    if (isOver()) {
        return;
    }
    _impl->status = Status::Draw;
    _impl->reason = EndReason::Agreement;
}

bool ChessGame::isOver() const
{
    return _impl->status != Status::InProgress;
}

ChessGame::Status ChessGame::status() const
{
    return _impl->status;
}

ChessGame::EndReason ChessGame::endReason() const
{
    return _impl->reason;
}

bool ChessGame::whiteWon() const
{
    return _impl->whiteWon;
}

bool ChessGame::blackWon() const
{
    return _impl->blackWon;
}

bool ChessGame::inCheck() const
{
    return _impl->board.inCheck();
}

bool ChessGame::whiteToMove() const
{
    return _impl->board.sideToMove() == chess::Color::WHITE;
}

std::string ChessGame::fen() const
{
    return _impl->board.getFen();
}

std::vector<std::string> ChessGame::legalMoves() const
{
    std::vector<std::string> result;

    if (isOver()) {
        return result;
    }

    chess::Movelist moves;
    chess::movegen::legalmoves(moves, _impl->board);

    for (const auto& move : moves) {
        result.push_back(chess::uci::moveToUci(move));
    }

    return result;
}

const std::vector<std::string>& ChessGame::history() const
{
    return _impl->uciMoves;
}

char ChessGame::pieceAt(int square) const
{
    if (square < 0 || square >= 64) {
        return '.';
    }

    const auto piece = _impl->board.at(chess::Square(square));
    if (piece == chess::Piece::NONE) {
        return '.';
    }

    const int type = static_cast<int>(piece.type());
    char c = '?';

    if (type == static_cast<int>(chess::PieceType::PAWN))        c = 'p';
    else if (type == static_cast<int>(chess::PieceType::KNIGHT)) c = 'n';
    else if (type == static_cast<int>(chess::PieceType::BISHOP)) c = 'b';
    else if (type == static_cast<int>(chess::PieceType::ROOK))   c = 'r';
    else if (type == static_cast<int>(chess::PieceType::QUEEN))  c = 'q';
    else if (type == static_cast<int>(chess::PieceType::KING))   c = 'k';
    else                                                         return '.';

    return piece.color() == chess::Color::WHITE ? static_cast<char>(std::toupper(c)) : c;
}

int ChessGame::kingSquare(bool white) const
{
    for (int square = 0; square < 64; ++square) {
        const auto piece = _impl->board.at(chess::Square(square));
        if (piece != chess::Piece::NONE && piece.type() == chess::PieceType::KING
            && piece.color() == (white ? chess::Color::WHITE : chess::Color::BLACK)) {
            return square;
        }
    }
    return -1;
}

bool ChessGame::replay(const std::vector<std::string>& moves)
{
    for (const auto& uci : moves) {
        if (!makeMove(uci)) {
            return false;
        }
    }
    return true;
}
