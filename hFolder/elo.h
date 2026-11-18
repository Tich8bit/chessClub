#pragma once

#include "match.h"
#include "ratings.h"

// Классический рейтинг Эло.
// K зависит от контроля времени: блиц 40, рапид 30, классика 20.
namespace Elo {

// Дисциплина рейтинга: 0 — классика, 1 — рапид, 2 — блиц.
enum class Discipline { Classical = 0, Rapid = 1, Blitz = 2 };

int kFactor(int timeControlId);

int deltaFor(int rating, int opponentRating, double score, int k);

// Применяет сыгранный матч: пересчитывает рейтинг обоих игроков
// в указанной дисциплине и учитывает результат в счётчиках В/Н/П.
void applyMatch(const Match& match, int k, Discipline discipline);

} // namespace Elo
