#include "chessClub.h"

#include <algorithm>
#include <map>
#include <stdexcept>

#include "chessgame.h"
#include "elo.h"
#include "exceptions/exception.h"
#include "player.h"

ChessClub::ChessClub(std::string_view name)
    : _clubName(name)
{
}

std::string_view ChessClub::getName() const { return _clubName; }
size_t ChessClub::getPlayerCount() const { return _players.size(); }
size_t ChessClub::getEventCount() const { return _events.size(); }
size_t ChessClub::getMatchCount() const { return _matches.size(); }

const Collection<Member>& ChessClub::getPlayers() const { return _players; }
const Collection<Event>& ChessClub::getEvents() const { return _events; }
const Collection<Match>& ChessClub::getMatches() const { return _matches; }

int ChessClub::nextPlayerId() const
{
    int id = 1;
    while (findPlayerById(id)) {
        ++id;
    }
    return id;
}

int ChessClub::nextEventId() const
{
    int id = 1;
    while (findEventById(id)) {
        ++id;
    }
    return id;
}

int ChessClub::nextMatchId() const
{
    int id = 1;
    while (findMatchById(id)) {
        ++id;
    }
    return id;
}

void ChessClub::addPlayer(std::shared_ptr<Member> player) { _players.add(player); }
void ChessClub::addEvent(std::shared_ptr<Event> event) { _events.add(event); }
void ChessClub::addMatch(std::shared_ptr<Match> match) { _matches.add(match); }

std::shared_ptr<Member> ChessClub::findPlayerById(int id) const
{
    return _players.find([id](const std::shared_ptr<Member>& player) {
        return player->getId() == id;
    });
}

std::shared_ptr<Event> ChessClub::findEventById(int id) const
{
    return _events.find([id](const std::shared_ptr<Event>& event) {
        return event->getId() == id;
    });
}

std::shared_ptr<Match> ChessClub::findMatchById(int id) const
{
    return _matches.find([id](const std::shared_ptr<Match>& match) {
        return match->getId() == id;
    });
}

std::shared_ptr<Tournament> ChessClub::findTournamentById(int id) const
{
    return std::dynamic_pointer_cast<Tournament>(findEventById(id));
}

bool ChessClub::removePlayer(int id)
{
    if (!findPlayerById(id)) {
        return false;
    }

    _players.removeIf([id](const std::shared_ptr<Member>& player) {
        return player->getId() == id;
    });

    for (const auto& event : _events) {
        if (auto tournament = std::dynamic_pointer_cast<Tournament>(event)) {
            tournament->removeParticipant(id);
            tournament->removeMatchesOf(id);
        }
    }

    _matches.removeIf([id](const std::shared_ptr<Match>& match) {
        return match->getWhitePlayer()->getId() == id
            || match->getBlackPlayer()->getId() == id;
    });

    recalculateStatistics();
    return true;
}

bool ChessClub::removeEvent(int id)
{
    auto event = findEventById(id);
    if (!event) {
        return false;
    }

    if (auto tournament = std::dynamic_pointer_cast<Tournament>(event)) {
        std::vector<int> matchIds;
        for (const auto& match : tournament->getMatches()) {
            matchIds.push_back(match->getId());
        }
        for (int matchId : matchIds) {
            removeMatch(matchId);
        }
    }

    _events.removeIf([id](const std::shared_ptr<Event>& item) {
        return item->getId() == id;
    });

    recalculateStatistics();
    return true;
}

bool ChessClub::removeMatch(int id)
{
    if (!findMatchById(id)) {
        return false;
    }

    _matches.removeIf([id](const std::shared_ptr<Match>& match) {
        return match->getId() == id;
    });

    for (const auto& event : _events) {
        if (auto tournament = std::dynamic_pointer_cast<Tournament>(event)) {
            tournament->removeMatch(id);
        }
    }

    recalculateStatistics();
    return true;
}

bool ChessClub::archivePlayer(int id)
{
    auto member = findPlayerById(id);
    if (!member) {
        return false;
    }
    member->setArchived(true);
    return true;
}

bool ChessClub::unarchivePlayer(int id)
{
    auto member = findPlayerById(id);
    if (!member) {
        return false;
    }
    member->setArchived(false);
    return true;
}

bool ChessClub::enrollPlayer(int tournamentId, int playerId)
{
    auto tournament = findTournamentById(tournamentId);
    if (!tournament) {
        return false;
    }

    auto member = findPlayerById(playerId);
    if (!member || !std::dynamic_pointer_cast<Player>(member) || member->isArchived()) {
        return false;
    }

    if (tournament->hasParticipant(playerId)) {
        return false;
    }

    try {
        *tournament += member;
    } catch (const ChessException&) {
        return false;
    }

    return true;
}

bool ChessClub::withdrawPlayer(int tournamentId, int playerId)
{
    auto tournament = findTournamentById(tournamentId);
    if (!tournament) {
        return false;
    }

    // Партии снявшегося игрока удаляем: иначе он «вернётся» в таблицу через матчи.
    std::vector<int> matchIds;
    for (const auto& match : tournament->getMatches()) {
        if (match->getWhitePlayer()->getId() == playerId
            || match->getBlackPlayer()->getId() == playerId) {
            matchIds.push_back(match->getId());
        }
    }
    for (int matchId : matchIds) {
        removeMatch(matchId);
    }

    return tournament->removeParticipant(playerId);
}

void ChessClub::createMatch(int id, std::shared_ptr<Player> white, std::shared_ptr<Player> black, const DateTime& date)
{
    _matches.add(std::make_shared<Match>(id, white, black, date));
}

bool ChessClub::createMatchInTournament(int matchId, int tournamentId, int whiteId, int blackId, const DateTime& date)
{
    auto tournament = findTournamentById(tournamentId);
    if (!tournament) {
        return false;
    }

    auto white = std::dynamic_pointer_cast<Player>(findPlayerById(whiteId));
    auto black = std::dynamic_pointer_cast<Player>(findPlayerById(blackId));
    if (!white || !black || whiteId == blackId) {
        return false;
    }

    if (!tournament->hasParticipant(whiteId) || !tournament->hasParticipant(blackId)) {
        return false;
    }

    auto match = std::make_shared<Match>(matchId, white, black, date);
    match->setTournamentId(tournamentId);
    _matches.add(match);
    *tournament += match;

    return true;
}

bool ChessClub::setMatchResult(int matchId, MatchResult::Result result, int kOverride)
{
    auto match = findMatchById(matchId);
    if (!match || match->getResult().isPlayed()) {
        return false;
    }

    // Если партия ведётся ходами, результат руками не вводится.
    if (match->hasGame()) {
        return false;
    }

    match->setResult(result);
    applyElo(*match, kOverride);

    return true;
}

void ChessClub::applyElo(Match& match, int kOverride)
{
    int k = 20;
    Elo::Discipline discipline = Elo::Discipline::Classical;

    if (auto tournament = findTournamentById(match.getTournamentId())) {
        switch (tournament->getTimeControl()) {
        case Tournament::TimeControl::Blitz:
            discipline = Elo::Discipline::Blitz;
            break;
        case Tournament::TimeControl::Rapid:
            discipline = Elo::Discipline::Rapid;
            break;
        case Tournament::TimeControl::Classic:
            discipline = Elo::Discipline::Classical;
            break;
        }
        k = Elo::kFactor(static_cast<int>(tournament->getTimeControl()));
    }

    if (kOverride > 0) {
        k = kOverride;
    }

    Elo::applyMatch(match, k, discipline);
    recalculateStatistics();
}

void ChessClub::applyGameResult(Match& match)
{
    auto game = match.game();
    if (!game || !game->isOver()) {
        return;
    }

    if (game->whiteWon()) {
        match.setResult(MatchResult::Result::WhiteWin);
    } else if (game->blackWon()) {
        match.setResult(MatchResult::Result::BlackWin);
    } else {
        match.setResult(MatchResult::Result::Draw);
    }

    applyElo(match);
}

bool ChessClub::playMove(int matchId, const std::string& uci)
{
    auto match = findMatchById(matchId);
    if (!match || !match->hasGame() || match->getResult().isPlayed()) {
        return false;
    }

    if (!match->game()->makeMove(uci)) {
        return false;
    }

    if (match->game()->isOver()) {
        applyGameResult(*match);
    }

    return true;
}

bool ChessClub::resignMatch(int matchId, bool whiteSide)
{
    auto match = findMatchById(matchId);
    if (!match || !match->hasGame() || match->getResult().isPlayed()) {
        return false;
    }

    match->game()->resign(whiteSide);
    applyGameResult(*match);
    return true;
}

bool ChessClub::agreeDraw(int matchId)
{
    auto match = findMatchById(matchId);
    if (!match || !match->hasGame() || match->getResult().isPlayed()) {
        return false;
    }

    match->game()->agreeDraw();
    applyGameResult(*match);
    return true;
}

void ChessClub::recalculateStatistics()
{
    for (const auto& member : _players) {
        member->resetStatistics();
    }

    for (const auto& match : _matches) {
        const auto result = match->getResult();
        if (!result.isPlayed()) {
            continue;
        }
        match->getWhitePlayer()->recordGame(result.getWhiteScore());
        match->getBlackPlayer()->recordGame(result.getBlackScore());
    }
}

std::vector<std::shared_ptr<Member>> ChessClub::sortedPlayers(SortKey key, bool ascending) const
{
    std::vector<std::shared_ptr<Member>> result(_players.begin(), _players.end());

    const auto comparator = [key](const std::shared_ptr<Member>& lhs,
                                  const std::shared_ptr<Member>& rhs) {
        switch (key) {
        case SortKey::ById:     return lhs->getId() < rhs->getId();
        case SortKey::ByName:   return lhs->getFullName() < rhs->getFullName();
        case SortKey::ByRating: return lhs->calculateMetric() < rhs->calculateMetric();
        case SortKey::ByScore:  return lhs->getScore() < rhs->getScore();
        }
        return false;
    };

    std::sort(result.begin(), result.end(), comparator);
    if (!ascending) {
        std::reverse(result.begin(), result.end());
    }

    return result;
}

std::vector<std::shared_ptr<Match>> ChessClub::matchesOfPlayer(int playerId) const
{
    std::vector<std::shared_ptr<Match>> result;

    for (const auto& match : _matches) {
        if (match->getWhitePlayer()->getId() == playerId
            || match->getBlackPlayer()->getId() == playerId) {
            result.push_back(match);
        }
    }

    return result;
}

void ChessClub::renumberIds()
{
    std::map<int, int> eventMap;

    std::vector<std::shared_ptr<Event>> events(_events.begin(), _events.end());
    std::sort(events.begin(), events.end(),
              [](const std::shared_ptr<Event>& a, const std::shared_ptr<Event>& b) {
                  return a->getId() < b->getId();
              });

    int nextEvent = 1;
    for (const auto& event : events) {
        eventMap[event->getId()] = nextEvent;
        event->_id = nextEvent;
        ++nextEvent;
    }

    std::vector<std::shared_ptr<Member>> members(_players.begin(), _players.end());
    std::sort(members.begin(), members.end(),
              [](const std::shared_ptr<Member>& a, const std::shared_ptr<Member>& b) {
                  return a->getId() < b->getId();
              });

    int nextMember = 1;
    for (const auto& member : members) {
        member->_id = nextMember;
        ++nextMember;
    }

    std::vector<std::shared_ptr<Match>> matches(_matches.begin(), _matches.end());
    std::sort(matches.begin(), matches.end(),
              [](const std::shared_ptr<Match>& a, const std::shared_ptr<Match>& b) {
                  return a->getId() < b->getId();
              });

    int nextMatch = 1;
    for (const auto& match : matches) {
        const int oldTournament = match->getTournamentId();
        match->_id = nextMatch;
        ++nextMatch;

        if (oldTournament > 0) {
            const auto it = eventMap.find(oldTournament);
            match->setTournamentId(it != eventMap.end() ? it->second : -1);
        }
    }
}

void ChessClub::printAllPlayers() const
{
    std::cout << "\nУчастники:\n";
    _players.print();
}

void ChessClub::printAllEvents() const
{
    std::cout << "\nСобытия:\n";
    _events.print();
}

void ChessClub::printAllMatches() const
{
    std::cout << "\nМатчи:\n";
    _matches.print();
}

ChessClub& ChessClub::operator+=(std::shared_ptr<Member> player)
{
    addPlayer(player);
    return *this;
}

ChessClub& ChessClub::operator+=(std::shared_ptr<Event> event)
{
    addEvent(event);
    return *this;
}

ChessClub& ChessClub::operator+=(std::shared_ptr<Match> match)
{
    addMatch(match);
    return *this;
}
