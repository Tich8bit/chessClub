#pragma once

#include <QDate>
#include <QDialog>
#include <QString>
#include <QTime>

class QComboBox;
class QDateEdit;
class QLineEdit;
class QSpinBox;
class QTimeEdit;

class AddPlayerDialog : public QDialog
{
public:
    explicit AddPlayerDialog(QWidget *parent = nullptr);

    QString lastName;
    QString firstName;
    QString middleName;
    int classical = 1000;
    int rapid = 1000;
    int blitz = 1000;

protected:
    void accept() override;

private:
    QLineEdit *m_lastName = nullptr;
    QLineEdit *m_firstName = nullptr;
    QLineEdit *m_middleName = nullptr;
    QSpinBox *m_classical = nullptr;
    QSpinBox *m_rapid = nullptr;
    QSpinBox *m_blitz = nullptr;
};

class AddCoachDialog : public QDialog
{
public:
    explicit AddCoachDialog(QWidget *parent = nullptr);

    QString lastName;
    QString firstName;
    QString middleName;
    int classical = 1000;
    int rapid = 1000;
    int blitz = 1000;
    int experience = 0;
    int specialization = 1;
    int qualification = 1;

protected:
    void accept() override;

private:
    QLineEdit *m_lastName = nullptr;
    QLineEdit *m_firstName = nullptr;
    QLineEdit *m_middleName = nullptr;
    QSpinBox *m_classical = nullptr;
    QSpinBox *m_rapid = nullptr;
    QSpinBox *m_blitz = nullptr;
    QSpinBox *m_experience = nullptr;
    QComboBox *m_specialization = nullptr;
    QSpinBox *m_qualification = nullptr;
};

class AddTournamentDialog : public QDialog
{
public:
    explicit AddTournamentDialog(QWidget *parent = nullptr);

    QString title;
    int timeControl = 2;   // 0 блиц, 1 рапид, 2 классика
    int system = 0;        // 0 круговая, 1 швейцарская, 2 плей-офф
    int maxPlayers = 8;

protected:
    void accept() override;

private:
    QLineEdit *m_title = nullptr;
    QComboBox *m_timeControl = nullptr;
    QComboBox *m_system = nullptr;
    QSpinBox *m_maxPlayers = nullptr;
};

class CreateMatchDialog : public QDialog
{
public:
    explicit CreateMatchDialog(QWidget *parent = nullptr);

    int tournamentId = 0;
    int whiteId = 1;
    int blackId = 1;
    QDate date = QDate::currentDate();
    QTime time = QTime::currentTime();

private:
    QSpinBox *m_tournamentId = nullptr;
    QSpinBox *m_whiteId = nullptr;
    QSpinBox *m_blackId = nullptr;
    QDateEdit *m_date = nullptr;
    QTimeEdit *m_time = nullptr;
};

class ResultDialog : public QDialog
{
public:
    explicit ResultDialog(QWidget *parent = nullptr);

    // 0 — ничья, 1 — победа белых, 2 — победа чёрных.
    int result = 0;

private:
    QComboBox *m_result = nullptr;
};
