#include "tournament.h"
#include "player.h"
#include "exceptions/exception.h"
#include <algorithm>
#include <map>

Tournament::Tournament(int id, std::string_view title, TimeControl timeControl, std::shared_ptr<TournamentSystem> system, int maxPlayers)
    : Event(id, title), _timeControl(timeControl), _system(system), _maxPlayers(maxPlayers) {
    _system->validateRules(_maxPlayers);
}

Tournament::TimeControl Tournament::getTimeControl() const { return _timeControl; }
int Tournament::getMaxPlayers() const { return _maxPlayers; }
const Collection<Member>& Tournament::getParticipants() const { return _participants; }
const Collection<Match>& Tournament::getMatches() const { return _matches; }
std::shared_ptr<TournamentSystem> Tournament::getSystem() const { return _system; }

std::string Tournament::getType() const { return "Турнир"; }
double Tournament::calculateMetric() const { return _system->calculateOptimalRounds(_maxPlayers); }
std::string Tournament::getMetricName() const { return "Игровая нагрузка"; }

void Tournament::printInfo(std::ostream& os) const {
    Event::printInfo(os);
    os << "\n  Система: " << _system->getName() << "\n  Участников: " << _participants.size() << "/" << _maxPlayers << "\n  Матчей: " << _matches.size() << "\n";
}

Tournament& Tournament::operator+=(std::shared_ptr<Member> player) {
    if (_participants.size() >= static_cast<size_t>(_maxPlayers)) throw LimitExceededException("заполнен");
    _participants.add(player);
    return *this;
}

Tournament& Tournament::operator-=(std::shared_ptr<Member> player) {
    _participants.removeIf([&player](const std::shared_ptr<Member>& p) { return p == player; });
    return *this;
}

Tournament& Tournament::operator+=(std::shared_ptr<Match> match) {
    _matches.add(match);
    return *this;
}

bool Tournament::hasParticipant(int memberId) const {
    return _participants.find([memberId](const std::shared_ptr<Member>& p) {
        return p->getId() == memberId;
    }) != nullptr;
}

bool Tournament::removeParticipant(int memberId) {
    return _participants.removeIf([memberId](const std::shared_ptr<Member>& p) {
        return p->getId() == memberId;
    }) > 0;
}

bool Tournament::removeMatch(int matchId) {
    return _matches.removeIf([matchId](const std::shared_ptr<Match>& m) {
        return m->getId() == matchId;
    }) > 0;
}

size_t Tournament::removeMatchesOf(int playerId) {
    return _matches.removeIf([playerId](const std::shared_ptr<Match>& m) {
        return m->getWhitePlayer()->getId() == playerId
            || m->getBlackPlayer()->getId() == playerId;
    });
}

std::vector<Standing> Tournament::getStandings(bool useHeadToHead) const {
    (void)useHeadToHead;  // личные встречи пока не учитываются
    std::map<std::shared_ptr<Member>, Standing> table;
    for (const auto& p : _participants) table[p] = {p, 0, 0, 0, 0.0};

    for (const auto& m : _matches) {
        auto res = m->getResult();
        if (!res.isPlayed()) continue;
        auto w = std::static_pointer_cast<Member>(m->getWhitePlayer());
        auto b = std::static_pointer_cast<Member>(m->getBlackPlayer());
        table[w].points += res.getWhiteScore();
        table[b].points += res.getBlackScore();
        if (res.getResult() == MatchResult::Result::WhiteWin) { table[w].wins++; table[b].losses++; }
        else if (res.getResult() == MatchResult::Result::BlackWin) { table[b].wins++; table[w].losses++; }
        else if (res.getResult() == MatchResult::Result::Draw) { table[w].draws++; table[b].draws++; }
    }

    std::vector<Standing> standings;
    for (const auto& [p, s] : table) standings.push_back(s);
    std::sort(standings.begin(), standings.end(), [](const Standing& a, const Standing& b) { return a.points > b.points; });
    return standings;
}