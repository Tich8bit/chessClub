#pragma once

#include <QMainWindow>
#include <QStringList>

#include <memory>

class ChessClub;
class ChessTableModel;
class QLineEdit;
class QSortFilterProxyModel;
class QTableView;
class QTabWidget;
class QVBoxLayout;

class MainWindow : public QMainWindow
{
    Q_OBJECT

public:
    explicit MainWindow(QWidget *parent = nullptr);
    ~MainWindow() override;

private slots:
    void addPlayer();
    void deletePlayer();
    void showPlayerHistory();

    void addCoach();
    void deleteCoach();

    void addTournament();
    void deleteTournament();
    void enrollPlayer();
    void withdrawPlayer();
    void showStandings();

    void addMatch();
    void setMatchResult();
    void deleteMatch();

    void saveState();
    void loadState();
    void makeReport();

    void applyTheme(const QString &name);
    void renumberIds();

private:
    struct Tab {
        ChessTableModel *model = nullptr;
        QSortFilterProxyModel *proxy = nullptr;
        QTableView *view = nullptr;
        QLineEdit *filter = nullptr;
    };

    void buildUi();
    QWidget *buildTableTab(Tab &tab, const QStringList &headers);
    void refreshAll();
    void refreshPlayers();
    void refreshCoaches();
    void refreshTournaments();
    void refreshMatches();

    int selectedId(const Tab &tab) const;
    void logAction(const QString &action);
    void message(const QString &text);

    std::unique_ptr<ChessClub> _club;

    Tab _playersTab;
    Tab _coachesTab;
    Tab _tournamentsTab;
    Tab _matchesTab;

    QTabWidget *_tabs = nullptr;
};
