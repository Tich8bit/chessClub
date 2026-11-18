#include "specialization.h"

Specialization::Specialization() : _type(Type::General) {}
Specialization::Specialization(Type type) : _type(type) {}

Specialization::Type Specialization::getType() const { return _type; }

Specialization::RatingType Specialization::getTargetRating() const {
    using enum Specialization::Type;
    using enum Specialization::RatingType;
    switch (_type) {
    case Openings:   return Classical;
    case Middlegame: return Rapid;
    case Endgame:    return Classical;
    case Tactics:    return Blitz;
    case General:    return All;
    }
    return All;
}

std::string Specialization::toString() const {
    using enum Specialization::Type;
    switch (_type) {
    case Openings:   return "Дебюты";
    case Middlegame: return "Миттельшпиль";
    case Endgame:    return "Эндшпиль";
    case Tactics:    return "Тактика";
    case General:    return "Общая подготовка";
    }
    return "Неизвестно";
}

Specialization::Type Specialization::intToType(int value) {
    using enum Specialization::Type;
    switch (value) {
    case 1: return Openings;
    case 2: return Middlegame;
    case 3: return Endgame;
    case 4: return Tactics;
    default: return General;
    }
}

std::ostream& operator<<(std::ostream& os, const Specialization& spec) {
    os << spec.toString();
    return os;
}