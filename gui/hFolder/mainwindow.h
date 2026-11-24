#pragma once

#include <QMainWindow>
#include <QString>
#include <QStringList>
#include <memory>

class QCheckBox;
class QLineEdit;
class QSortFilterProxyModel;
class QTabWidget;
class QTableView;
class QWidget;

class ChessClub;
class ChessTableModel;

class MainWindow : public QMainWindow
{
    Q_OBJECT

public:
    explicit MainWindow(const QString &clubName, QWidget *parent = nullptr);
    explicit MainWindow(QWidget *parent = nullptr);
    ~MainWindow() override;

    static int startupChoice();
    void applyStartup(int mode);

private slots:
    void addPlayer();
    void showPlayerHistory();
    void deletePlayer();

    void addCoach();
    void deleteCoach();

    void addTournament();
    void deleteTournament();
    void enrollPlayer();
    void withdrawPlayer();
    void showStandings();

    void addMatch();
    void playMatch();
    void setMatchResult();
    void deleteMatch();

    void saveState();
    void makeReport();
    void viewReport();
    void viewLog();
    void applyTheme(const QString &name);

private:
    struct Tab {
        ChessTableModel *model = nullptr;
        QSortFilterProxyModel *proxy = nullptr;
        QTableView *view = nullptr;
        QLineEdit *filter = nullptr;
        QCheckBox *showArchive = nullptr;
    };

    void buildUi();
    QWidget *buildTableTab(Tab &tab, const QStringList &headers);
    void openTextFileDialog(const QString &title, const QString &path);

    void refreshAll();
    void refreshPlayers();
    void refreshCoaches();
    void refreshTournaments();
    void refreshMatches();

    int selectedId(const Tab &tab) const;
    void setArchivedForSelected(const Tab &tab, bool archived);
    void logAction(const QString &action);
    void message(const QString &text);

    std::unique_ptr<ChessClub> _club;
    QTabWidget *_tabs = nullptr;
    Tab _playersTab;
    Tab _coachesTab;
    Tab _tournamentsTab;
    Tab _matchesTab;
};