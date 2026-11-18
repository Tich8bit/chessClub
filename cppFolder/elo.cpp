#include "elo.h"

#include <algorithm>
#include <cmath>

#include "player.h"

namespace {

double expectedScore(int rating, int opponentRating)
{
    return 1.0 / (1.0 + std::pow(10.0, (opponentRating - rating) / 400.0));
}

int pickRating(const Ratings& ratings, Elo::Discipline discipline)
{
    using enum Elo::Discipline;
    switch (discipline) {
    case Rapid:     return ratings.getRapid();
    case Blitz:     return ratings.getBlitz();
    case Classical:
    default:        return ratings.getClassical();
    }
}

Ratings withDelta(const Ratings& ratings, Elo::Discipline discipline, int delta)
{
    using enum Elo::Discipline;
    auto clamp = [](int value) { return std::clamp(value, 0, 3000); };

    int classical = ratings.getClassical();
    int rapid = ratings.getRapid();
    int blitz = ratings.getBlitz();

    switch (discipline) {
    case Rapid:     rapid = clamp(rapid + delta); break;
    case Blitz:     blitz = clamp(blitz + delta); break;
    case Classical: classical = clamp(classical + delta); break;
    }

    return Ratings(classical, rapid, blitz);
}

} // namespace

int Elo::kFactor(int timeControlId)
{
    // Tournament::TimeControl: 0 — Blitz, 1 — Rapid, 2 — Classic.
    switch (timeControlId) {
    case 0:  return 40;
    case 1:  return 30;
    default: return 20;
    }
}

int Elo::deltaFor(int rating, int opponentRating, double score, int k)
{
    const double expected = expectedScore(rating, opponentRating);
    return static_cast<int>(std::lround(k * (score - expected)));
}

void Elo::applyMatch(const Match& match, int k, Discipline discipline)
{
    if (!match.getResult().isPlayed()) {
        return;
    }

    auto white = match.getWhitePlayer();
    auto black = match.getBlackPlayer();
    if (!white || !black) {
        return;
    }

    const Ratings whiteRatings = white->getRatings();
    const Ratings blackRatings = black->getRatings();

    const int whiteRating = pickRating(whiteRatings, discipline);
    const int blackRating = pickRating(blackRatings, discipline);

    const double whiteScore = match.getResult().getWhiteScore();
    const double blackScore = match.getResult().getBlackScore();

    const int whiteDelta = deltaFor(whiteRating, blackRating, whiteScore, k);
    const int blackDelta = deltaFor(blackRating, whiteRating, blackScore, k);

    white->setRatings(withDelta(whiteRatings, discipline, whiteDelta));
    black->setRatings(withDelta(blackRatings, discipline, blackDelta));
}
