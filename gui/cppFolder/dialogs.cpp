#include "dialogs.h"

#include <QComboBox>
#include <QDateEdit>
#include <QDialogButtonBox>
#include <QFormLayout>
#include <QLineEdit>
#include <QMessageBox>
#include <QSpinBox>
#include <QTimeEdit>
#include <QVBoxLayout>

namespace {

QDialogButtonBox *makeButtons(QDialog *dialog)
{
    auto *buttons = new QDialogButtonBox(QDialogButtonBox::Ok | QDialogButtonBox::Cancel, dialog);
    QObject::connect(buttons, &QDialogButtonBox::accepted, dialog, &QDialog::accept);
    QObject::connect(buttons, &QDialogButtonBox::rejected, dialog, &QDialog::reject);
    return buttons;
}

QSpinBox *makeRatingSpin(QWidget *parent, int value)
{
    auto *spin = new QSpinBox(parent);
    spin->setRange(0, 3000);
    spin->setValue(value);
    return spin;
}

} // namespace

AddPlayerDialog::AddPlayerDialog(QWidget *parent)
    : QDialog(parent)
{
    setWindowTitle("Новый игрок");

    auto *form = new QFormLayout;
    m_lastName = new QLineEdit(this);
    m_firstName = new QLineEdit(this);
    m_middleName = new QLineEdit(this);
    m_classical = makeRatingSpin(this, 1000);
    m_rapid = makeRatingSpin(this, 1000);
    m_blitz = makeRatingSpin(this, 1000);

    form->addRow("Фамилия:", m_lastName);
    form->addRow("Имя:", m_firstName);
    form->addRow("Отчество:", m_middleName);
    form->addRow("Рейтинг (классика):", m_classical);
    form->addRow("Рейтинг (рапид):", m_rapid);
    form->addRow("Рейтинг (блиц):", m_blitz);

    auto *layout = new QVBoxLayout(this);
    layout->addLayout(form);
    layout->addWidget(makeButtons(this));
}

void AddPlayerDialog::accept()
{
    if (m_lastName->text().trimmed().isEmpty() || m_firstName->text().trimmed().isEmpty()) {
        QMessageBox::warning(this, "Проверка", "Фамилия и имя обязательны.");
        return;
    }

    lastName = m_lastName->text().trimmed();
    firstName = m_firstName->text().trimmed();
    middleName = m_middleName->text().trimmed();
    classical = m_classical->value();
    rapid = m_rapid->value();
    blitz = m_blitz->value();

    QDialog::accept();
}

AddCoachDialog::AddCoachDialog(QWidget *parent)
    : QDialog(parent)
{
    setWindowTitle("Новый тренер");

    auto *form = new QFormLayout;
    m_lastName = new QLineEdit(this);
    m_firstName = new QLineEdit(this);
    m_middleName = new QLineEdit(this);
    m_classical = makeRatingSpin(this, 1000);
    m_rapid = makeRatingSpin(this, 1000);
    m_blitz = makeRatingSpin(this, 1000);
    m_experience = new QSpinBox(this);
    m_experience->setRange(0, 80);
    m_qualification = new QSpinBox(this);
    m_qualification->setRange(1, 5);

    m_specialization = new QComboBox(this);
    m_specialization->addItems({"Дебюты", "Миттельшпиль", "Эндшпиль", "Тактика", "Общая"});

    form->addRow("Фамилия:", m_lastName);
    form->addRow("Имя:", m_firstName);
    form->addRow("Отчество:", m_middleName);
    form->addRow("Рейтинг (классика):", m_classical);
    form->addRow("Рейтинг (рапид):", m_rapid);
    form->addRow("Рейтинг (блиц):", m_blitz);
    form->addRow("Стаж (лет):", m_experience);
    form->addRow("Специализация:", m_specialization);
    form->addRow("Квалификация:", m_qualification);

    auto *layout = new QVBoxLayout(this);
    layout->addLayout(form);
    layout->addWidget(makeButtons(this));
}

void AddCoachDialog::accept()
{
    if (m_lastName->text().trimmed().isEmpty() || m_firstName->text().trimmed().isEmpty()) {
        QMessageBox::warning(this, "Проверка", "Фамилия и имя обязательны.");
        return;
    }

    lastName = m_lastName->text().trimmed();
    firstName = m_firstName->text().trimmed();
    middleName = m_middleName->text().trimmed();
    classical = m_classical->value();
    rapid = m_rapid->value();
    blitz = m_blitz->value();
    experience = m_experience->value();
    specialization = m_specialization->currentIndex() + 1;
    qualification = m_qualification->value();

    QDialog::accept();
}

AddTournamentDialog::AddTournamentDialog(QWidget *parent)
    : QDialog(parent)
{
    setWindowTitle("Новый турнир");

    auto *form = new QFormLayout;
    m_title = new QLineEdit(this);

    m_timeControl = new QComboBox(this);
    m_timeControl->addItems({"Блиц", "Рапид", "Классика"});
    m_timeControl->setCurrentIndex(2);

    m_system = new QComboBox(this);
    m_system->addItems({"Круговая", "Швейцарская", "Плей-офф"});

    m_maxPlayers = new QSpinBox(this);
    m_maxPlayers->setRange(2, 200);
    m_maxPlayers->setValue(8);

    form->addRow("Название:", m_title);
    form->addRow("Контроль времени:", m_timeControl);
    form->addRow("Система:", m_system);
    form->addRow("Макс. игроков:", m_maxPlayers);

    auto *layout = new QVBoxLayout(this);
    layout->addLayout(form);
    layout->addWidget(makeButtons(this));
}

void AddTournamentDialog::accept()
{
    if (m_title->text().trimmed().isEmpty()) {
        QMessageBox::warning(this, "Проверка", "Название обязательно.");
        return;
    }

    title = m_title->text().trimmed();
    timeControl = m_timeControl->currentIndex();
    system = m_system->currentIndex();
    maxPlayers = m_maxPlayers->value();

    QDialog::accept();
}

CreateMatchDialog::CreateMatchDialog(QWidget *parent)
    : QDialog(parent)
{
    setWindowTitle("Новая партия");

    auto *form = new QFormLayout;
    m_tournamentId = new QSpinBox(this);
    m_tournamentId->setRange(0, 99999);
    m_tournamentId->setSpecialValueText("вне турнира");

    m_whiteId = new QSpinBox(this);
    m_whiteId->setRange(1, 99999);
    m_blackId = new QSpinBox(this);
    m_blackId->setRange(1, 99999);
    m_blackId->setValue(2);

    m_date = new QDateEdit(QDate::currentDate(), this);
    m_date->setDisplayFormat("dd.MM.yyyy");
    m_time = new QTimeEdit(QTime::currentTime(), this);
    m_time->setDisplayFormat("HH:mm");

    form->addRow("ID турнира:", m_tournamentId);
    form->addRow("ID белого:", m_whiteId);
    form->addRow("ID чёрного:", m_blackId);
    form->addRow("Дата:", m_date);
    form->addRow("Время:", m_time);

    auto *layout = new QVBoxLayout(this);
    layout->addLayout(form);
    layout->addWidget(makeButtons(this));
}

ResultDialog::ResultDialog(QWidget *parent)
    : QDialog(parent)
{
    setWindowTitle("Результат партии");

    m_result = new QComboBox(this);
    m_result->addItems({"Ничья", "Победа белых", "Победа чёрных"});

    auto *layout = new QVBoxLayout(this);
    layout->addWidget(m_result);
    auto *buttons = makeButtons(this);
    QObject::connect(buttons, &QDialogButtonBox::accepted, this, [this]() {
        result = m_result->currentIndex();
        QDialog::accept();
    });
    layout->addWidget(buttons);
}
