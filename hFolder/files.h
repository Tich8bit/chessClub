#pragma once

#include <fstream>
#include <string>

#include "chessClub.h"
#include "collection.h"

class StorageManager {
public:
    static void saveState(const std::string& filename, const ChessClub& club);
    static void loadState(const std::string& filename, ChessClub& club);

    static void logAction(const std::string& logFilename, const std::string& action);

    static void generateReport(const std::string& reportFilename, const ChessClub& club);

private:
    static void savePlayers(std::ofstream& ofs, const ChessClub& club);
    static void saveEvents(std::ofstream& ofs, const ChessClub& club);
    static void saveMatches(std::ofstream& ofs, const ChessClub& club);

    static void loadPlayer(std::ifstream& ifs, ChessClub& club, bool withArchive);
    static void loadCoach(std::ifstream& ifs, ChessClub& club, bool withArchive);
    static void loadTournament(std::ifstream& ifs, ChessClub& club, bool withParticipants);
    static void loadTraining(std::ifstream& ifs, ChessClub& club);
    static int loadMatch(std::ifstream& ifs, ChessClub& club, bool withTournamentId, bool withMoves, int prevMatchId);

    static void reportPlayers(std::ofstream& ofs, const ChessClub& club);
    static void reportEvents(std::ofstream& ofs, const ChessClub& club);
    static void reportMatches(std::ofstream& ofs, const ChessClub& club);
    static void reportStandings(std::ofstream& ofs, const ChessClub& club);
    static void reportArchive(std::ofstream& ofs, const ChessClub& club);

    static constexpr const char* HEADER = "CHESSDB4";
    static constexpr const char* PREFIX_PLAYER     = "PLR";
    static constexpr const char* PREFIX_COACH      = "CHC";
    static constexpr const char* PREFIX_TOURNAMENT = "TRN";
    static constexpr const char* PREFIX_TRAINING   = "TNG";
    static constexpr const char* PREFIX_MATCH      = "MTC";
};