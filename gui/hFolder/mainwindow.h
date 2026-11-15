#pragma once

#include <QMainWindow>

#include <memory>

class ChessClub;
class QLabel;
class QListWidget;

class MainWindow : public QMainWindow
{
    Q_OBJECT

public:
    explicit MainWindow(QWidget *parent = nullptr);
    ~MainWindow() override;

private:
    void refresh();

    std::unique_ptr<ChessClub> _club;
    QLabel *_summary = nullptr;
    QListWidget *_players = nullptr;
};
