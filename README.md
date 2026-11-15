# chessClub — система шахматного клуба

Учёт игроков и тренеров, турниры, тренировки, матчи, рейтинги и аналитика.
Одно ядро и две сборки: консольная и графическая на Qt 6 Widgets.

## Структура

```
chessClub/
├─ CMakeLists.txt          сборка: chessCore + chessClub + gui
├─ cppFolder/              ядро, реализация (main.cpp — точка входа консоли)
├─ hFolder/                ядро, заголовки
│   └─ exceptions/         иерархия исключений
├─ gui/
│   ├─ CMakeLists.txt
│   ├─ cppFolder/          реализация GUI (main.cpp, mainwindow.cpp, dialogs/, models/)
│   └─ hFolder/            заголовки GUI (mainwindow.h, dialogs/, models/)
├─ club.txt                сохранённое состояние клуба
└─ README.md
```

Правило проекта: реализация — в `cppFolder`, заголовки — в `hFolder`, на любом уровне.
Ядро (`cppFolder` + `hFolder`) не содержит Qt и собирается отдельно.

## Консольная версия

Из корня проекта:

```
g++ -std=c++20 cppFolder\*.cpp -IhFolder -o chessClub.exe
chessClub.exe
```

Стартовое меню: `1` — тестовые данные, `2` — загрузка из `club.txt`, `0` — выход.
Дальше: 1.Игрок 2.Тренер 3.Турнир 4.Тренировка 5.Матч 6.Таблица 7.Сохранить 8.Показать 0.Выход.

## GUI на Qt 6

Требуется Qt 6.12 с китом MinGW 13.1, CMake и Ninja (всё есть в `C:\Qt\Tools`).

Конфигурация и сборка (cmd, из корня проекта):

```
cmake -S . -B build -G Ninja ^
  -DCMAKE_PREFIX_PATH=C:/Qt/6.12.0/mingw_64 ^
  -DCMAKE_CXX_COMPILER=C:/Qt/Tools/mingw1310_64/bin/g++.exe ^
  -DCMAKE_MAKE_PROGRAM=C:/Qt/Tools/Ninja/ninja.exe ^
  -DCMAKE_BUILD_TYPE=Debug

cmake --build build
```

Исполняемый файл: `build\gui\chessClubGui.exe`.

Запуск: рядом с `.exe` нужны библиотеки Qt, поэтому либо запускай из Qt Creator
(он подставляет PATH сам), либо выполни один раз `windeployqt build\gui\chessClubGui.exe`,
либо добавь `C:\Qt\6.12.0\mingw_64\bin` в `PATH`.

В Qt Creator проще всего открыть сам `CMakeLists.txt` (File → Open File or Project)
и выбрать кит «Desktop Qt 6.12.0 MinGW 64-bit».

## Заметки

- `club.txt` ищется в текущем каталоге — запускай из корня проекта.
- Исходники в UTF-8 без BOM; консольная версия выставляет `CP_UTF8`.
- В коде есть `#include "basefunction.h"`, а файл называется `baseFunction.h`.
  На Windows работает, на Linux/case-sensitive ФС — нет.
- GUI находится в development-стадии: пока открывается окно со списком игроков.
