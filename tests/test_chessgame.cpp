#include <iostream>
#include <string>
#include <vector>

#include "chessgame.h"

int main()
{
    ChessGame game;

    std::cout << "Legal moves at start: " << game.legalMoves().size() << "\n";
    std::cout << "FEN: " << game.fen() << "\n\n";

    // Мат Легаля (Scholar's mate).
    const std::vector<std::string> moves = {
        "e2e4", "e7e5", "f1c4", "b8c6",
        "d1h5", "g8f6", "h5f7"
    };

    for (const auto& move : moves) {
        const bool ok = game.makeMove(move);
        std::cout << move << " -> " << (ok ? "ok" : "REJECTED")
                  << " | status: " << static_cast<int>(game.status())
                  << " | check: " << (game.inCheck() ? "yes" : "no")
                  << " | legal: " << game.legalMoves().size() << "\n";
    }

    std::cout << "\nResult: whiteWon=" << game.whiteWon()
              << " blackWon=" << game.blackWon()
              << " over=" << game.isOver() << "\n";
    std::cout << "History size: " << game.history().size() << "\n";
    std::cout << "FEN after: " << game.fen() << "\n";

    std::cout << "\nUndo one move -> status: " << static_cast<int>(game.status()) << "\n";

    return 0;
}
