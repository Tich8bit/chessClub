#include "chessgame.h"

#include <string>
#include <vector>

#include "chessboard.h"
#include "chessmove.h"
#include "chesspiece.h"

struct ChessGame::Impl {
    ChessBoard board;
    std::vector<std::string> uciMoves;

    Status status = Status::InProgress;
    EndReason reason = EndReason::None;
    bool whiteWon = false;
    bool blackWon = false;

    void resetState()
    {
        board.reset();
        uciMoves.clear();
        status = Status::InProgress;
        reason = EndReason::None;
        whiteWon = false;
        blackWon = false;
    }

    void updateEndState()
    {
        if (status == Status::Resigned || (status == Status::Draw && reason == EndReason::Agreement)) {
            return;
        }

        const auto state = board.evaluateState();
        if (state == ChessBoard::PositionState::Checkmate) {
            status = Status::Checkmate;
            reason = EndReason::Checkmate;
            if (board.whiteToMove()) {
                whiteWon = false;
                blackWon = true;
            } else {
                whiteWon = true;
                blackWon = false;
            }
        } else if (state == ChessBoard::PositionState::Stalemate) {
            status = Status::Draw;
            reason = EndReason::Stalemate;
            whiteWon = false;
            blackWon = false;
        } else if (state == ChessBoard::PositionState::InsufficientMaterial) {
            status = Status::Draw;
            reason = EndReason::InsufficientMaterial;
            whiteWon = false;
            blackWon = false;
        } else {
            status = Status::InProgress;
            reason = EndReason::None;
            whiteWon = false;
            blackWon = false;
        }
    }
};

ChessGame::ChessGame()
    : _impl(std::make_unique<Impl>())
{
    _impl->resetState();
}

ChessGame::~ChessGame() = default;

bool ChessGame::makeMove(const std::string& uci)
{
    if (isOver()) {
        return false;
    }

    const ChessMove move = ChessMove::fromUci(uci);
    if (move.from < 0 || move.to < 0) {
        return false;
    }

    if (!_impl->board.makeMove(move)) {
        return false;
    }

    _impl->uciMoves.push_back(move.toUci());
    _impl->updateEndState();
    return true;
}

bool ChessGame::undoMove()
{
    if (_impl->uciMoves.empty() || _impl->status == Status::Resigned) {
        return false;
    }

    if (!_impl->board.undoMove()) {
        return false;
    }

    _impl->uciMoves.pop_back();
    _impl->updateEndState();
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
    _impl->whiteWon = false;
    _impl->blackWon = false;
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
    return _impl->board.inCheck(_impl->board.whiteToMove());
}

bool ChessGame::whiteToMove() const
{
    return _impl->board.whiteToMove();
}

std::string ChessGame::fen() const
{
    return _impl->board.toFen();
}

std::vector<std::string> ChessGame::legalMoves() const
{
    std::vector<std::string> result;
    if (isOver()) {
        return result;
    }

    const auto moves = _impl->board.legalMoves();
    result.reserve(moves.size());
    for (const auto& m : moves) {
        result.push_back(m.toUci());
    }
    return result;
}

const std::vector<std::string>& ChessGame::history() const
{
    return _impl->uciMoves;
}

char ChessGame::pieceAt(int square) const
{
    return _impl->board.pieceAt(square).toChar();
}

int ChessGame::kingSquare(bool white) const
{
    return _impl->board.kingSquare(white);
}

bool ChessGame::replay(const std::vector<std::string>& moves)
{
    _impl->resetState();
    for (const auto& uci : moves) {
        if (!makeMove(uci)) {
            return false;
        }
    }
    return true;
}