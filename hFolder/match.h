#pragma once
#include <memory>
#include <string>
#include "matchResult.h"
#include "dateTime.h"

class Player;
class ChessGame;

class Match {
    friend class ChessClub;
public:
    Match(int id, std::shared_ptr<Player> whitePlayer, std::shared_ptr<Player> blackPlayer, const DateTime& date);
    int getId() const;
    std::shared_ptr<Player> getWhitePlayer() const;
    std::shared_ptr<Player> getBlackPlayer() const;
    const DateTime& getDate() const;
    MatchResult getResult() const;
    void setResult(MatchResult::Result newResult);

    // Турнир, в рамках которого играется партия (-1 — вне турнира).
    int getTournamentId() const;
    void setTournamentId(int tournamentId);

    // Партия с ходами. nullptr — импортированный матч без истории.
    bool hasGame() const;
    std::shared_ptr<ChessGame> game() const;
    void setGame(std::shared_ptr<ChessGame> game);
    friend std::ostream& operator<<(std::ostream& os, const Match& m);
private:
    int _id;
    std::shared_ptr<Player> _whitePlayer;
    std::shared_ptr<Player> _blackPlayer;
    DateTime _date;
    MatchResult _result{MatchResult::Result::NotPlayed};
    int _tournamentId = -1;
    std::shared_ptr<ChessGame> _game;
};