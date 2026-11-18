#include "files.h"

#include <algorithm>
#include <array>
#include <chrono>
#include <ctime>
#include <format>
#include <iostream>
#include <sstream>

#include "coach.h"
#include "chessgame.h"
#include "exceptions/exception.h"
#include "matchResult.h"
#include "player.h"
#include "specialization.h"
#include "tournament.h"
#include "tournamentSystem.h"
#include "trainingSession.h"

void StorageManager::saveState(const std::string& filename, const ChessClub& club) {
    std::ofstream ofs(filename);
    if (!ofs.is_open()) throw InvalidOperationException("не удалось открыть файл для сохранения");

    ofs << HEADER << "\n";
    savePlayers(ofs, club);
    saveEvents(ofs, club);
    saveMatches(ofs, club);
}

void StorageManager::savePlayers(std::ofstream& ofs, const ChessClub& club) {
    const auto& players = club.getPlayers();
    ofs << players.size() << "\n";

    for (const auto& p : players) {
        auto firstName  = std::string(p->getFirstName());
        auto lastName   = std::string(p->getLastName());
        auto middleName = std::string(p->getMiddleName());

        if (auto coach = std::dynamic_pointer_cast<Coach>(p)) {
            int specInt = std::to_underlying(coach->getSpecialization().getType());
            ofs << PREFIX_COACH << " " << coach->getId() << " " << std::quoted(lastName) << " "
                << std::quoted(firstName) << " " << std::quoted(middleName) << " "
                << p->getRatings().getClassical() << " " << p->getRatings().getRapid() << " "
                << p->getRatings().getBlitz() << " " << p->getWins() << " " << p->getDraws() << " "
                << p->getLosses() << " " << coach->getExperienceYears() << " "
                << specInt << " " << coach->getQualificationLevel() << " "
                << (p->isArchived() ? 1 : 0) << "\n";
        } else {
            ofs << PREFIX_PLAYER << " " << p->getId() << " " << std::quoted(lastName) << " "
                << std::quoted(firstName) << " " << std::quoted(middleName) << " "
                << p->getRatings().getClassical() << " " << p->getRatings().getRapid() << " "
                << p->getRatings().getBlitz() << " " << p->getWins() << " " << p->getDraws() << " "
                << p->getLosses() << " " << (p->isArchived() ? 1 : 0) << "\n";
        }
    }
}

void StorageManager::saveEvents(std::ofstream& ofs, const ChessClub& club) {
    const auto& events = club.getEvents();
    ofs << events.size() << "\n";

    for (const auto& e : events) {
        if (auto t = std::dynamic_pointer_cast<Tournament>(e)) {
            auto title = std::string(t->getTitle());
            ofs << PREFIX_TOURNAMENT << " " << t->getId() << " " << std::quoted(title) << " "
                << std::to_underlying(t->getTimeControl()) << " " << t->getSystem()->getTypeId() << " "
                << t->getMaxPlayers() << " " << t->getParticipants().size();

            for (const auto& participant : t->getParticipants()) {
                ofs << " " << participant->getId();
            }
            ofs << "\n";
        } else if (auto ts = std::dynamic_pointer_cast<TrainingSession>(e)) {
            auto title = std::string(ts->getTitle());
            auto topic = std::string(ts->getTopic());
            int coachId = -1;
            if (auto coach = ts->getCoach()) coachId = coach->getId();
            ofs << PREFIX_TRAINING << " " << ts->getId() << " " << std::quoted(title) << " "
                << std::quoted(topic) << " " << ts->getDurationMinutes() << " " << coachId << "\n";
        }
    }
}

void StorageManager::saveMatches(std::ofstream& ofs, const ChessClub& club) {
    const auto& matches = club.getMatches();
    ofs << matches.size() << "\n";

    for (const auto& m : matches) {
        std::vector<std::string> moves;
        if (m->hasGame() && m->game()) {
            moves = m->game()->history();
        }

        ofs << PREFIX_MATCH << " " << m->getId() << " " << m->getWhitePlayer()->getId() << " "
            << m->getBlackPlayer()->getId() << " " << std::quoted(m->getDate().toString()) << " "
            << std::to_underlying(m->getResult().getResult()) << " "
            << m->getTournamentId() << " " << moves.size();

        for (const auto& uci : moves) {
            ofs << " " << uci;
        }
        ofs << "\n";
    }
}

void StorageManager::loadState(const std::string& filename, ChessClub& club) {
    std::ifstream ifs(filename);
    if (!ifs.is_open()) throw InvalidOperationException("не удалось открыть файл для загрузки");

    std::string header;
    ifs >> header;

    // CHESSDB4 — ходы партий, CHESSDB3 — архив, CHESSDB2 — ссылки на турниры.
    const bool withMoves = (header == "CHESSDB4");
    const bool withArchive = withMoves || (header == "CHESSDB3");
    const bool withExtras = withArchive || (header == "CHESSDB2");
    if (!withExtras) {
        ifs.clear();
        ifs.seekg(0);
    }

    int playersCount = 0;
    ifs >> playersCount;
    for (int i = 0; i < playersCount; ++i) {
        std::string prefix;
        ifs >> prefix;
        if (prefix == PREFIX_PLAYER) loadPlayer(ifs, club, withArchive);
        else if (prefix == PREFIX_COACH) loadCoach(ifs, club, withArchive);
        else throw InvalidDataException("неверный префикс участника: " + prefix);
    }

    int eventsCount = 0;
    ifs >> eventsCount;
    for (int i = 0; i < eventsCount; ++i) {
        std::string prefix;
        ifs >> prefix;
        if (prefix == PREFIX_TOURNAMENT) loadTournament(ifs, club, withExtras);
        else if (prefix == PREFIX_TRAINING) loadTraining(ifs, club);
        else throw InvalidDataException("неверный префикс события: " + prefix);
    }

    int matchesCount = 0;
    ifs >> matchesCount;
    for (int i = 0; i < matchesCount; ++i) loadMatch(ifs, club, withExtras, withMoves);

    if (!ifs) throw InvalidDataException("файл повреждён: неожиданный конец данных");

    // Приводим идентификаторы к 1..N и пересчитываем статистику из партий.
    club.renumberIds();
    club.recalculateStatistics();
}

void StorageManager::loadPlayer(std::ifstream& ifs, ChessClub& club, bool withArchive) {
    int id, classical, rapid, blitz, wins, draws, losses;
    std::string lastName, firstName, middleName;

    if (!(ifs >> id >> std::quoted(lastName) >> std::quoted(firstName) >> std::quoted(middleName)
              >> classical >> rapid >> blitz >> wins >> draws >> losses)) {
        throw InvalidDataException("ошибка чтения данных игрока");
    }

    int archived = 0;
    if (withArchive && !(ifs >> archived)) {
        throw InvalidDataException("ошибка чтения флага архива игрока");
    }

    auto player = std::make_shared<Player>(id, firstName, lastName, middleName,
        Ratings(classical, rapid, blitz), wins, draws, losses);
    player->setArchived(archived != 0);
    club.addPlayer(player);
}

void StorageManager::loadCoach(std::ifstream& ifs, ChessClub& club, bool withArchive) {
    int id, classical, rapid, blitz, wins, draws, losses, experienceYears, specInt, qualificationLevel;
    std::string lastName, firstName, middleName;

    if (!(ifs >> id >> std::quoted(lastName) >> std::quoted(firstName) >> std::quoted(middleName)
              >> classical >> rapid >> blitz >> wins >> draws >> losses
              >> experienceYears >> specInt >> qualificationLevel)) {
        throw InvalidDataException("ошибка чтения данных тренера");
    }

    int archived = 0;
    if (withArchive && !(ifs >> archived)) {
        throw InvalidDataException("ошибка чтения флага архива тренера");
    }

    auto coach = std::make_shared<Coach>(id, firstName, lastName, middleName,
        Ratings(classical, rapid, blitz), wins, draws, losses,
        experienceYears, Specialization(static_cast<Specialization::Type>(specInt)), qualificationLevel);
    coach->setArchived(archived != 0);
    club.addPlayer(coach);
}

void StorageManager::loadTournament(std::ifstream& ifs, ChessClub& club, bool withParticipants) {
    int id, tc, sysId, maxPlayers;
    std::string title;

    if (!(ifs >> id >> std::quoted(title) >> tc >> sysId >> maxPlayers)) {
        throw InvalidDataException("ошибка чтения турнира");
    }

    auto system = TournamentSystem::createFromId(sysId);
    auto tournament = std::make_shared<Tournament>(id, title,
        static_cast<Tournament::TimeControl>(tc), system, maxPlayers);

    if (withParticipants) {
        size_t participantsCount = 0;
        if (!(ifs >> participantsCount)) {
            throw InvalidDataException("ошибка чтения участников турнира");
        }

        for (size_t i = 0; i < participantsCount; ++i) {
            int participantId = 0;
            if (!(ifs >> participantId)) {
                throw InvalidDataException("ошибка чтения участника турнира");
            }
            if (auto member = club.findPlayerById(participantId)) {
                try {
                    *tournament += member;
                } catch (const ChessException&) {
                    // Лимит участников меньше сохранённого — пропускаем.
                }
            }
        }
    }

    club.addEvent(tournament);
}

void StorageManager::loadTraining(std::ifstream& ifs, ChessClub& club) {
    int id, durationMinutes, coachId;
    std::string title, topic;

    if (!(ifs >> id >> std::quoted(title) >> std::quoted(topic) >> durationMinutes >> coachId)) {
        throw InvalidDataException("ошибка чтения тренировки");
    }

    std::shared_ptr<Coach> coach = nullptr;
    if (coachId != -1) {
        auto found = club.findPlayerById(coachId);
        if (!found) throw RelationException("тренер не найден");
        coach = std::dynamic_pointer_cast<Coach>(found);
        if (!coach) throw RelationException("участник не является тренером");
    }

    club.addEvent(std::make_shared<TrainingSession>(id, title, topic, durationMinutes, coach));
}

void StorageManager::loadMatch(std::ifstream& ifs, ChessClub& club, bool withTournamentId, bool withMoves) {
    std::string prefix, dateStr;
    int id, whiteId, blackId, resultInt;

    if (!(ifs >> prefix >> id >> whiteId >> blackId >> std::quoted(dateStr) >> resultInt)) {
        throw InvalidDataException("ошибка чтения партии");
    }
    if (prefix != PREFIX_MATCH) throw InvalidDataException("неверный префикс партии");

    int tournamentId = -1;
    if (withTournamentId && !(ifs >> tournamentId)) {
        throw InvalidDataException("ошибка чтения турнира партии");
    }

    std::vector<std::string> moves;
    if (withMoves) {
        size_t movesCount = 0;
        if (!(ifs >> movesCount)) {
            throw InvalidDataException("ошибка чтения количества ходов");
        }
        moves.reserve(movesCount);
        for (size_t i = 0; i < movesCount; ++i) {
            std::string uci;
            if (!(ifs >> uci)) {
                throw InvalidDataException("ошибка чтения хода");
            }
            moves.push_back(uci);
        }
    }

    int d, m, y, h = 0, min = 0, s = 0;
    char dot1, dot2;
    std::stringstream ss(dateStr);
    DateTime date;

    if (ss >> d >> dot1 >> m >> dot2 >> y) {
        if (ss >> h) {
            char colon1, colon2;
            ss >> colon1 >> min >> colon2 >> s;
        }
        date = DateTime(d, m, y, h, min, s);
    } else {
        throw InvalidDataException("неверный формат даты в файле: " + dateStr);
    }

    auto whiteMember = club.findPlayerById(whiteId);
    auto blackMember = club.findPlayerById(blackId);
    auto white = std::dynamic_pointer_cast<Player>(whiteMember);
    auto black = std::dynamic_pointer_cast<Player>(blackMember);

    if (!white || !black) throw RelationException("участник не является игроком");

    auto match = std::make_shared<Match>(id, white, black, date);
    match->setResult(static_cast<MatchResult::Result>(resultInt));
    match->setTournamentId(tournamentId);

    if (withMoves && !moves.empty()) {
        auto game = std::make_shared<ChessGame>();
        if (game->replay(moves)) {
            match->setGame(game);
        } else {
            match->setGame(nullptr);
        }
    } else {
        // Создаём новую игровую доску для партий без истории ходов
        match->setGame(std::make_shared<ChessGame>());
    }

    club.addMatch(match);

    if (tournamentId > 0) {
        if (auto tournament = club.findTournamentById(tournamentId)) {
            *tournament += match;
        }
    }
}

void StorageManager::generateReport(const std::string& reportFilename, const ChessClub& club) {
    std::ofstream ofs(reportFilename);
    if (!ofs.is_open()) throw InvalidOperationException("не удалось открыть файл для отчёта");

    ofs << "ОТЧЁТ О СОСТОЯНИИ ШАХМАТНОГО КЛУБА\n";
    ofs << "==================================\n\n";

    reportPlayers(ofs, club);
    reportEvents(ofs, club);
    reportMatches(ofs, club);
    reportStandings(ofs, club);
    reportArchive(ofs, club);
}

void StorageManager::reportPlayers(std::ofstream& ofs, const ChessClub& club) {
    const auto& players = club.getPlayers();
    ofs << "УЧАСТНИКИ (" << players.size() << "):\n";

    for (const auto& p : players) {
        ofs << "  - [" << p->getRole() << "] " << p->getFullName()
            << " | ID: " << p->getId()
            << (p->isArchived() ? " | АРХИВ" : "")
            << " | " << p->getRatings()
            << " | В/Н/П: " << p->getWins() << "/" << p->getDraws() << "/" << p->getLosses()
            << " | Очки: " << p->getScore() << "\n";
    }
    ofs << "\n";
}

void StorageManager::reportEvents(std::ofstream& ofs, const ChessClub& club) {
    const auto& events = club.getEvents();
    ofs << "СОБЫТИЯ (" << events.size() << "):\n";

    for (const auto& e : events) {
        ofs << "  - [" << e->getType() << "] " << e->getTitle() << " | ID: " << e->getId() << "\n";

        if (auto t = std::dynamic_pointer_cast<Tournament>(e)) {
            ofs << "      Система: " << t->getSystem()->getName()
                << " | Участников: " << t->getParticipants().size() << "/" << t->getMaxPlayers()
                << " | Партий: " << t->getMatches().size() << "\n";
        }
    }
    ofs << "\n";
}

void StorageManager::reportMatches(std::ofstream& ofs, const ChessClub& club) {
    const auto& matches = club.getMatches();
    ofs << "ПАРТИИ (" << matches.size() << "):\n";

    for (const auto& m : matches) {
        ofs << "  - " << m->getWhitePlayer()->getFullName()
            << " vs " << m->getBlackPlayer()->getFullName()
            << " | Дата: " << m->getDate()
            << " | Результат: " << m->getResult() << "\n";
    }
    ofs << "\n";
}

void StorageManager::reportStandings(std::ofstream& ofs, const ChessClub& club) {
    for (const auto& e : club.getEvents()) {
        auto tournament = std::dynamic_pointer_cast<Tournament>(e);
        if (!tournament) continue;

        ofs << "ТУРНИРНАЯ ТАБЛИЦА: " << tournament->getTitle() << "\n";
        const auto standings = tournament->getStandings(true);

        if (standings.empty()) {
            ofs << "  (нет участников)\n\n";
            continue;
        }

        for (size_t i = 0; i < standings.size(); ++i) {
            const auto& row = standings[i];
            ofs << "  " << (i + 1) << ". " << row.player->getFullName()
                << " | Очки: " << row.points
                << " | В/Н/П: " << row.wins << "/" << row.draws << "/" << row.losses << "\n";
        }
        ofs << "\n";
    }
}

void StorageManager::reportArchive(std::ofstream& ofs, const ChessClub& club) {
    std::vector<std::shared_ptr<Member>> archived;

    for (const auto& member : club.getPlayers()) {
        if (member->isArchived()) {
            archived.push_back(member);
        }
    }

    if (archived.empty()) {
        return;
    }

    ofs << "АРХИВ (" << archived.size() << "):\n";
    for (const auto& member : archived) {
        ofs << "  - [" << member->getRole() << "] " << member->getFullName()
            << " | ID: " << member->getId()
            << " | " << member->getRatings() << "\n";
    }
    ofs << "\n";
}

void StorageManager::logAction(const std::string& logFilename, const std::string& action) {
    std::ofstream ofs(logFilename, std::ios::app);
    if (!ofs.is_open()) throw InvalidOperationException("не удалось открыть лог-файл");

    const auto now = std::chrono::system_clock::now();
    std::tm timeInfo{};
    localtime_s(&timeInfo, std::chrono::system_clock::to_time_t(now));

    const std::string timestamp = std::format("{:04d}-{:02d}-{:02d} {:02d}:{:02d}:{:02d}",
                                              timeInfo.tm_year + 1900, timeInfo.tm_mon + 1, timeInfo.tm_mday,
                                              timeInfo.tm_hour, timeInfo.tm_min, timeInfo.tm_sec);

    ofs << "[" << timestamp << "] " << action << "\n";
}
