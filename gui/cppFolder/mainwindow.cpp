#include "mainwindow.h"

#include <QLabel>
#include <QListWidget>
#include <QVBoxLayout>
#include <QWidget>

#include "chessClub.h"
#include "player.h"

MainWindow::MainWindow(QWidget *parent)
    : QMainWindow(parent)
    , _club(std::make_unique<ChessClub>("Гамбит"))
{
    setWindowTitle("chessClub");
    resize(820, 560);

    auto *central = new QWidget(this);
    auto *layout = new QVBoxLayout(central);

    _summary = new QLabel(central);
    _players = new QListWidget(central);

    layout->addWidget(_summary);
    layout->addWidget(_players, 1);

    setCentralWidget(central);

    // Демонстрационные данные: проверяем, что ядро линкуется и работает.
    auto player = std::make_shared<Player>(
        1, "Иван", "Иванов", "Иванович",
        Ratings(1200, 1250, 1300), 5, 2, 3);
    _club->addPlayer(player);

    refresh();
}

MainWindow::~MainWindow() = default;

void MainWindow::refresh()
{
    _summary->setText(QString("%1 — игроков: %2, событий: %3, матчей: %4")
                          .arg(QString::fromStdString(std::string(_club->getName())))
                          .arg(_club->getPlayerCount())
                          .arg(_club->getEventCount())
                          .arg(_club->getMatchCount()));

    _players->clear();
    for (const auto &member : _club->getPlayers()) {
        const QString name = QString::fromStdString(member->getFullName());
        const QString role = QString::fromStdString(member->getRole());
        const double metric = member->calculateMetric();
        _players->addItem(QString("%1  [%2]  %3: %4")
                              .arg(name, role,
                                   QString::fromStdString(member->getMetricName()))
                              .arg(metric));
    }
}
