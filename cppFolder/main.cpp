#include <Windows.h>

#include <iostream>
#include <locale>
#include <memory>
#include <sstream>
#include <string>

#include "baseFunction.h"
#include "chessClub.h"
#include "coach.h"
#include "exceptions/exception.h"
#include "files.h"
#include "match.h"
#include "player.h"
#include "ratings.h"
#include "specialization.h"
#include "tournament.h"
#include "trainingSession.h"

using namespace std;

namespace {

const char *STATE_FILE  = "club.txt";
const char *LOG_FILE    = "journal.log";
const char *REPORT_FILE = "report.txt";

void logAction(const string &action) {
    try {
        StorageManager::logAction(LOG_FILE, action);
    } catch (const ChessException &) {
        // Журнал не должен ломать основную операцию.
    }
}

DateTime inputDateTime() {
    while (true) {
        string str = inputString("Дата (ДД.ММ.ГГГГ ЧЧ:ММ:СС): ");
        int d, m, y, h = 0, min = 0, s = 0;
        char dot1, dot2;
        stringstream ss(str);

        if (ss >> d >> dot1 >> m >> dot2 >> y && dot1 == '.' && dot2 == '.') {
            if (ss >> h) {
                char c1, c2;
                if (!(ss >> c1 >> min >> c2 >> s) || c1 != ':' || c2 != ':') {
                    cout << "Ошибка времени\n";
                    continue;
                }
            }
            try {
                return DateTime(d, m, y, h, min, s);
            } catch (const ChessException &e) {
                cout << e.what() << "\n";
            }
        } else {
            cout << "Ошибка формата\n";
        }
    }
}

int chooseStartMode() {
    cout << "_________________________________________\n";
    cout << "|        Система шахматного клуба       |\n";
    cout << "|_______________________________________|\n";
    cout << "|1. Создать тестовую систему            |\n";
    cout << "|2. Загрузить из club.txt               |\n";
    cout << "|3. Создать новый клуб                  |\n";
    cout << "|0. Выход                               |\n";
    cout << "|_______________________________________|\n";
    return inputNumber<int>("Ваш выбор: ", 0, 3);
}

void printMainMenu() {
    cout << "_________________________________________\n";
    cout << "|          Управление клубом            |\n";
    cout << "|_______________________________________|\n";
    cout << "|1.  Добавить игрока                    |\n";
    cout << "|2.  Добавить тренера                   |\n";
    cout << "|3.  Добавить турнир                    |\n";
    cout << "|4.  Добавить тренировку                |\n";
    cout << "|5.  Создать партию                     |\n";
    cout << "|6.  Турнирная таблица                  |\n";
    cout << "|8.  Показать всё                       |\n";
    cout << "|9.  Удалить объект                     |\n";
    cout << "|10. Записать игрока на турнир          |\n";
    cout << "|11. Ввести результат партии            |\n";
    cout << "|13. Перенумеровать ID                  |\n";
    cout << "|14. Архив участника                    |\n";
    cout << "|_______________________________________|\n";
    cout << "|             ЛР — Файлы                |\n";
    cout << "|_______________________________________|\n";
    cout << "|7.  Сохранить в club.txt               |\n";
    cout << "|12. Сформировать отчёт                 |\n";
    cout << "|_______________________________________|\n";
    cout << "|0.  Выход                              |\n";
    cout << "|_______________________________________|\n";
}

void createTestSystem(ChessClub &club) {
    club = ChessClub("Гамбит");

    auto p1 = make_shared<Player>(1, "Иван", "Иванов", "Иванович", Ratings(1200, 1250, 1300), 5, 2, 3);
    auto p2 = make_shared<Player>(2, "Пётр", "Петров", "Петрович", Ratings(1800, 1750, 1700), 10, 4, 6);
    auto c1 = make_shared<Coach>(3, "Тимофей", "Тренеров", "Тимофеевич", Ratings(2000, 1950, 1900),
                                 15, 5, 10, 25, Specialization(Specialization::Type::Openings), 4);

    club += p1;
    club += p2;
    club += c1;

    auto sys = make_shared<RoundRobinSystem>(1, true);
    auto t1 = make_shared<Tournament>(1, "Чемпионат", Tournament::TimeControl::Classic, sys, 12);
    *t1 += p1;
    *t1 += p2;
    club += t1;

    auto m1 = make_shared<Match>(1, p1, p2, DateTime(1, 11, 2026, 14, 30, 0));
    m1->setTournamentId(1);
    *t1 += m1;
    club.addMatch(m1);

    club += make_shared<TrainingSession>(2, "Дебюты", "Испанская", 90, c1);

    cout << "Тестовая система создана.\n";
}

void demoAddPlayer(ChessClub &club) {
    const int id = club.nextPlayerId();
    cout << "Новый ID: " << id << "\n";

    string fn = inputString("Имя: ");
    string ln = inputString("Фамилия: ");
    string mn = inputString("Отчество: ");

    int cl = inputNumber<int>("Классика: ", 0, 3000);
    int rp = inputNumber<int>("Рапид: ", 0, 3000);
    int bl = inputNumber<int>("Блиц: ", 0, 3000);

    club += make_shared<Player>(id, fn, ln, mn, Ratings(cl, rp, bl), 0, 0, 0);
    logAction("добавлен игрок ID=" + to_string(id) + " " + ln + " " + fn);
    cout << "Игрок добавлен.\n";
}

void demoAddCoach(ChessClub &club) {
    const int id = club.nextPlayerId();
    cout << "Новый ID: " << id << "\n";

    string fn = inputString("Имя: ");
    string ln = inputString("Фамилия: ");
    string mn = inputString("Отчество: ");

    int cl = inputNumber<int>("Классика: ", 0, 3000);
    int rp = inputNumber<int>("Рапид: ", 0, 3000);
    int bl = inputNumber<int>("Блиц: ", 0, 3000);
    int exp = inputNumber<int>("Стаж: ", 0, 80);

    cout << "1.Дебюты 2.Миттельшпиль 3.Эндшпиль 4.Тактика 5.Общая\n";
    int sc = inputNumber<int>("Спец: ", 1, 5);
    int q = inputNumber<int>("Квалификация: ", 1, 5);

    club += make_shared<Coach>(id, fn, ln, mn, Ratings(cl, rp, bl), 0, 0, 0,
                               exp, Specialization(Specialization::intToType(sc)), q);
    logAction("добавлен тренер ID=" + to_string(id) + " " + ln + " " + fn);
    cout << "Тренер добавлен.\n";
}

void demoAddTournament(ChessClub &club) {
    const int id = club.nextEventId();
    cout << "Новый ID: " << id << "\n";

    string title = inputString("Название: ");
    int tc = inputNumber<int>("Контроль (0-Блиц,1-Рапид,2-Классика): ", 0, 2);
    int sc = inputNumber<int>("Система (0-Круговая,1-Швейцарская,2-Плей-офф): ", 0, 2);
    int maxP = inputNumber<int>("Макс. игроков: ", 2, 200);

    shared_ptr<TournamentSystem> sys;
    if (sc == 0) sys = make_shared<RoundRobinSystem>(1, true);
    else if (sc == 1) sys = make_shared<SwissSystem>(5, true);
    else sys = make_shared<KnockoutSystem>(false);

    try {
        club += make_shared<Tournament>(id, title, static_cast<Tournament::TimeControl>(tc), sys, maxP);
        logAction("добавлен турнир ID=" + to_string(id) + " " + title);
        cout << "Турнир добавлен.\n";
    } catch (const ChessException &e) {
        cout << e.what() << "\n";
    }
}

void demoAddTraining(ChessClub &club) {
    const int id = club.nextEventId();
    cout << "Новый ID: " << id << "\n";

    string title = inputString("Название: ");
    string topic = inputString("Тема: ");
    int dur = inputNumber<int>("Длительность: ", 10, 300);
    int coachId = inputNumber<int>("ID тренера: ", 1, 99999);

    auto coach = dynamic_pointer_cast<Coach>(club.findPlayerById(coachId));
    if (!coach) {
        cout << "Тренер не найден.\n";
        return;
    }

    club += make_shared<TrainingSession>(id, title, topic, dur, coach);
    logAction("добавлена тренировка ID=" + to_string(id) + " " + title);
    cout << "Тренировка добавлена.\n";
}

void demoCreateMatch(ChessClub &club) {
    const int id = club.nextMatchId();
    cout << "Новый ID партии: " << id << "\n";

    int tourId = inputNumber<int>("ID турнира (0 — вне турнира): ", 0, 99999);
    int wId = inputNumber<int>("ID белого: ", 1, 99999);
    int bId = inputNumber<int>("ID чёрного: ", 1, 99999);

    if (tourId > 0) {
        if (club.createMatchInTournament(id, tourId, wId, bId, inputDateTime())) {
            logAction("создана партия ID=" + to_string(id) + " в турнире " + to_string(tourId));
            cout << "Партия создана в турнире.\n";
        } else {
            cout << "Не удалось: турнир/игроки не найдены или игроки не записаны.\n";
        }
        return;
    }

    auto w = dynamic_pointer_cast<Player>(club.findPlayerById(wId));
    auto b = dynamic_pointer_cast<Player>(club.findPlayerById(bId));
    if (!w || !b) {
        cout << "Игроки не найдены.\n";
        return;
    }

    club.createMatch(id, w, b, inputDateTime());
    logAction("создана партия ID=" + to_string(id));
    cout << "Партия создана.\n";
}

void demoDelete(ChessClub &club) {
    int kind = inputNumber<int>("Удалить (1-игрока, 2-событие, 3-партию): ", 1, 3);
    int id = inputNumber<int>("ID: ", 1, 99999);

    bool removed = false;
    if (kind == 1) removed = club.removePlayer(id);
    else if (kind == 2) removed = club.removeEvent(id);
    else removed = club.removeMatch(id);

    if (removed) {
        logAction("удаление: тип=" + to_string(kind) + " ID=" + to_string(id));
        cout << "Удалено.\n";
    } else {
        cout << "Объект не найден.\n";
    }
}

void demoEnroll(ChessClub &club) {
    int tourId = inputNumber<int>("ID турнира: ", 1, 99999);
    int action = inputNumber<int>("1-записать, 2-снять: ", 1, 2);
    int playerId = inputNumber<int>("ID игрока: ", 1, 99999);

    const bool ok = (action == 1) ? club.enrollPlayer(tourId, playerId)
                                  : club.withdrawPlayer(tourId, playerId);

    if (ok) {
        logAction(string(action == 1 ? "запись на турнир " : "снятие с турнира ")
                  + to_string(tourId) + " игрока " + to_string(playerId));
        cout << "Готово.\n";
    } else {
        cout << "Не удалось (нет турнира/игрока, лимит или уже записан).\n";
    }
}

void demoSetResult(ChessClub &club) {
    int matchId = inputNumber<int>("ID партии: ", 1, 99999);
    int res = inputNumber<int>("Результат (0-ничья, 1-победа белых, 2-победа чёрных): ", 0, 2);

    if (club.setMatchResult(matchId, static_cast<MatchResult::Result>(res))) {
        logAction("результат партии " + to_string(matchId) + " = " + to_string(res));
        cout << "Результат записан, рейтинги пересчитаны.\n";
    } else {
        cout << "Партия не найдена или результат уже записан.\n";
    }
}

void demoStandings(const ChessClub &club) {
    int tid = inputNumber<int>("ID турнира: ", 1, 99999);
    auto t = club.findTournamentById(tid);
    if (!t) {
        cout << "Нет турнира.\n";
        return;
    }

    auto st = t->getStandings(true);
    if (st.empty()) {
        cout << "Нет участников.\n";
        return;
    }

    for (size_t i = 0; i < st.size(); ++i) {
        cout << i + 1 << ". " << st[i].player->getFullName()
             << " | Очки: " << st[i].points
             << " | В/Н/П: " << st[i].wins << "/" << st[i].draws << "/" << st[i].losses << "\n";
    }
}

void demoReport(const ChessClub &club) {
    try {
        StorageManager::generateReport(REPORT_FILE, club);
        cout << "Отчёт записан в " << REPORT_FILE << "\n";
    } catch (const ChessException &e) {
        cout << "Ошибка отчёта: " << e.what() << "\n";
    }
}

void demoRenumber(ChessClub &club) {
    club.renumberIds();
    logAction("перенумерация ID");
    cout << "ID перенумерованы: участники, события и партии — 1..N.\n";
}

void demoArchive(ChessClub &club) {
    int id = inputNumber<int>("ID участника: ", 1, 99999);
    int action = inputNumber<int>("1-в архив, 2-из архива: ", 1, 2);

    const bool ok = (action == 1) ? club.archivePlayer(id) : club.unarchivePlayer(id);
    if (ok) {
        logAction(std::string(action == 1 ? "в архив ID=" : "из архива ID=") + std::to_string(id));
        cout << "Готово.\n";
    } else {
        cout << "Участник не найден.\n";
    }
}

} // namespace

int main() {
    SetConsoleOutputCP(CP_UTF8);
    SetConsoleCP(CP_UTF8);
    setlocale(LC_ALL, "ru_RU.UTF-8");

    ChessClub club("Гамбит");

    const int mode = chooseStartMode();
    if (mode == 0) return 0;

    if (mode == 1) {
        createTestSystem(club);
    } else if (mode == 2) {
        try {
            StorageManager::loadState(STATE_FILE, club);
            cout << "Загружено из " << STATE_FILE << "\n";
        } catch (const ChessException &e) {
            cout << "Не удалось загрузить (" << e.what() << "), создаю тестовую систему.\n";
            createTestSystem(club);
        }
    } else {
        cout << "Создан новый клуб. Добавьте участников.\n";
    }

    while (true) {
        printMainMenu();
        int c = inputNumber<int>("Ваш выбор: ", 0, 14);
        if (c == 0) break;

        try {
            switch (c) {
            case 1:  demoAddPlayer(club); break;
            case 2:  demoAddCoach(club); break;
            case 3:  demoAddTournament(club); break;
            case 4:  demoAddTraining(club); break;
            case 5:  demoCreateMatch(club); break;
            case 6:  demoStandings(club); break;
            case 7:
                StorageManager::saveState(STATE_FILE, club);
                logAction("сохранение состояния");
                cout << "Сохранено.\n";
                break;
            case 8:  club.printAllPlayers(); club.printAllEvents(); club.printAllMatches(); break;
            case 9:  demoDelete(club); break;
            case 10: demoEnroll(club); break;
            case 11: demoSetResult(club); break;
            case 12: demoReport(club); break;
            case 13: demoRenumber(club); break;
            case 14: demoArchive(club); break;
            default: break;
            }
        } catch (const ChessException &e) {
            cout << "ОШИБКА: " << e.what() << "\n";
        }
    }

    return 0;
}
