#pragma once

#include <QMainWindow>
#include <QSet>
#include <QString>
#include <QStringList>
#include <memory>

class QComboBox;
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
    void finishTournament();
    void deleteTournament();
    void enrollPlayer();
    void withdrawPlayer();
    void showStandings();
    void showTournamentInfo();

    void addTraining();
    void deleteTraining();
    void enrollTrainingPlayer();
    void withdrawTrainingPlayer();
    void finishTraining();
    void cancelTraining();
    void showTrainingDetails();

    void addMatch();
    void playMatch();
    void setMatchResult();
    void deleteMatch();

    void saveState();
    void makeReport();
    void viewReport();
    void viewLog();

private:
    struct Tab {
        ChessTableModel *model = nullptr;
        QSortFilterProxyModel *proxy = nullptr;
        QTableView *view = nullptr;
        QLineEdit *filter = nullptr;
        QComboBox *statusFilter = nullptr;
    };

    void buildUi();
    void applyBlueTheme();
    QWidget *buildTableTab(Tab &tab, const QStringList &headers, bool withMatchStatusFilter = false);
    void openTextFileDialog(const QString &title, const QString &path);

    void populateTournamentParticipantsFromMatches();
    void autoPersist();

    bool isTournamentFinished(int tournamentId) const;
    QString tournamentStatusLabel(int tournamentId) const;

    QString trainingStatusLabel(int trainingId) const;
    bool isTrainingCompleted(int trainingId) const;
    bool isTrainingCancelled(int trainingId) const;

    void refreshAll();
    void refreshPlayers();
    void refreshCoaches();
    void refreshTournaments();
    void refreshTrainings();
    void refreshMatches();

    int selectedId(const Tab &tab) const;
    void logAction(const QString &action);
    void message(const QString &text);

    std::unique_ptr<ChessClub> _club;
    QTabWidget *_tabs = nullptr;
    Tab _playersTab;
    Tab _coachesTab;
    Tab _tournamentsTab;
    Tab _trainingsTab;
    Tab _matchesTab;

    QSet<int> _finishedTournaments;
    QSet<int> _completedTrainings;
    QSet<int> _cancelledTrainings;
};