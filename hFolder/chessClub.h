#pragma once

#include <algorithm>
#include <memory>
#include <string>
#include <string_view>
#include <vector>

#include "collection.h"
#include "event.h"
#include "match.h"
#include "member.h"
#include "tournament.h"

// Ключ сортировки списка участников.
enum class SortKey { ById, ByName, ByRating, ByScore };

class ChessClub {
public:
    explicit ChessClub(std::string_view name);

    std::string_view getName() const;
    size_t getPlayerCount() const;
    size_t getEventCount() const;
    size_t getMatchCount() const;

    const Collection<Member>& getPlayers() const;
    const Collection<Event>& getEvents() const;
    const Collection<Match>& getMatches() const;

    // Следующий свободный идентификатор.
    int nextPlayerId() const;
    int nextEventId() const;
    int nextMatchId() const;

    void addPlayer(std::shared_ptr<Member> player);
    void addEvent(std::shared_ptr<Event> event);
    void addMatch(std::shared_ptr<Match> match);

    std::shared_ptr<Member> findPlayerById(int id) const;
    std::shared_ptr<Event> findEventById(int id) const;
    std::shared_ptr<Match> findMatchById(int id) const;
    std::shared_ptr<Tournament> findTournamentById(int id) const;

    // Удаление с очисткой связей. Возвращает false, если объекта не было.
    bool removePlayer(int id);
    bool removeEvent(int id);
    bool removeMatch(int id);

    // Архив: скрыть участника, сохранив его партии и историю.
    bool archivePlayer(int id);
    bool unarchivePlayer(int id);

    // Запись игрока на турнир и обратно.
    bool enrollPlayer(int tournamentId, int playerId);
    bool withdrawPlayer(int tournamentId, int playerId);

    void createMatch(int id, std::shared_ptr<Player> white, std::shared_ptr<Player> black, const DateTime& date);

    // Создаёт партию внутри турнира; оба игрока должны быть записаны.
    bool createMatchInTournament(int matchId, int tournamentId, int whiteId, int blackId, const DateTime& date);

    // Фиксирует результат партии и пересчитывает Эло. Повторно нельзя.
    bool setMatchResult(int matchId, MatchResult::Result result, int kOverride = 0);

    // Ходы в партии (UCI: "e2e4"). Результат появляется автоматически, когда игра окончена.
    bool playMove(int matchId, const std::string& uci);
    bool resignMatch(int matchId, bool whiteSide);
    bool agreeDraw(int matchId);
    bool undoMove(int matchId);

    std::vector<std::shared_ptr<Member>> sortedPlayers(SortKey key, bool ascending = true) const;

    // Все партии участника (в турнирах и вне их).
    std::vector<std::shared_ptr<Match>> matchesOfPlayer(int playerId) const;

    // Перенумеровывает участников, события и партии как 1..N
    // и обновляет ссылки (турнир у партии).
    void renumberIds();

    // Пересчитывает В/Н/П по сыгранным партиям (партии — источник правды).
    void recalculateStatistics();

    void printAllPlayers() const;
    void printAllEvents() const;
    void printAllMatches() const;

    ChessClub& operator+=(std::shared_ptr<Member> player);
    ChessClub& operator+=(std::shared_ptr<Event> event);
    ChessClub& operator+=(std::shared_ptr<Match> match);

private:
    void applyElo(Match& match, int kOverride = 0);
    void applyGameResult(Match& match);

    std::string _clubName;
    Collection<Member> _players;
    Collection<Event> _events;
    Collection<Match> _matches;
};
