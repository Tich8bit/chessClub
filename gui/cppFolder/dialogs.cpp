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

#include <memory>
#include <utility>

namespace {

template <typename T, typename Parent, typename... Args>
T *makeOwned(Parent *parent, Args &&...args)
{
    auto ptr = std::make_unique<T>(std::forward<Args>(args)..., parent);
    return ptr.release();
}

template <typename T, typename... Args>
T *makeLayout(Args &&...args)
{
    auto ptr = std::make_unique<T>(std::forward<Args>(args)...);
    return ptr.release();
}

QDialogButtonBox *makeButtons(QDialog *dialog)
{
    auto *buttons = makeOwned<QDialogButtonBox>(dialog, QDialogButtonBox::Ok | QDialogButtonBox::Cancel);
    QObject::connect(buttons, &QDialogButtonBox::accepted, dialog, &QDialog::accept);
    QObject::connect(buttons, &QDialogButtonBox::rejected, dialog, &QDialog::reject);
    return buttons;
}

QSpinBox *makeRatingSpin(QWidget *parent, int value)
{
    auto *spin = makeOwned<QSpinBox>(parent);
    spin->setRange(0, 3000);
    spin->setValue(value);
    return spin;
}

} // namespace

AddPlayerDialog::AddPlayerDialog(QWidget *parent)
    : QDialog(parent)
{
    setWindowTitle("Новый игрок");

    auto *form = makeLayout<QFormLayout>();
    m_lastName = makeOwned<QLineEdit>(this);
    m_firstName = makeOwned<QLineEdit>(this);
    m_middleName = makeOwned<QLineEdit>(this);
    m_classical = makeRatingSpin(this, 1000);
    m_rapid = makeRatingSpin(this, 1000);
    m_blitz = makeRatingSpin(this, 1000);

    form->addRow("Фамилия:", m_lastName);
    form->addRow("Имя:", m_firstName);
    form->addRow("Отчество:", m_middleName);
    form->addRow("Рейтинг (классика):", m_classical);
    form->addRow("Рейтинг (рапид):", m_rapid);
    form->addRow("Рейтинг (блиц):", m_blitz);

    auto *layout = makeOwned<QVBoxLayout>(this);
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

    auto *form = makeLayout<QFormLayout>();
    m_lastName = makeOwned<QLineEdit>(this);
    m_firstName = makeOwned<QLineEdit>(this);
    m_middleName = makeOwned<QLineEdit>(this);
    m_classical = makeRatingSpin(this, 1000);
    m_rapid = makeRatingSpin(this, 1000);
    m_blitz = makeRatingSpin(this, 1000);
    m_experience = makeOwned<QSpinBox>(this);
    m_experience->setRange(0, 80);
    m_qualification = makeOwned<QSpinBox>(this);
    m_qualification->setRange(1, 5);

    m_specialization = makeOwned<QComboBox>(this);
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

    auto *layout = makeOwned<QVBoxLayout>(this);
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

    auto *form = makeLayout<QFormLayout>();
    m_title = makeOwned<QLineEdit>(this);

    m_timeControl = makeOwned<QComboBox>(this);
    m_timeControl->addItems({"Блиц", "Рапид", "Классика"});
    m_timeControl->setCurrentIndex(2);

    m_system = makeOwned<QComboBox>(this);
    m_system->addItems({"Круговая", "Швейцарская", "Плей-офф"});

    m_maxPlayers = makeOwned<QSpinBox>(this);
    m_maxPlayers->setRange(2, 200);
    m_maxPlayers->setValue(8);

    form->addRow("Название:", m_title);
    form->addRow("Контроль времени:", m_timeControl);
    form->addRow("Система:", m_system);
    form->addRow("Макс. игроков:", m_maxPlayers);

    auto *layout = makeOwned<QVBoxLayout>(this);
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

    auto *form = makeLayout<QFormLayout>();
    m_tournamentId = makeOwned<QSpinBox>(this);
    m_tournamentId->setRange(0, 99999);
    m_tournamentId->setSpecialValueText("вне турнира");

    m_whiteId = makeOwned<QSpinBox>(this);
    m_whiteId->setRange(1, 99999);
    m_blackId = makeOwned<QSpinBox>(this);
    m_blackId->setRange(1, 99999);
    m_blackId->setValue(2);

    m_date = makeOwned<QDateEdit>(this, QDate::currentDate());
    m_date->setDisplayFormat("dd.MM.yyyy");
    m_time = makeOwned<QTimeEdit>(this, QTime::currentTime());
    m_time->setDisplayFormat("HH:mm");

    form->addRow("ID турнира:", m_tournamentId);
    form->addRow("ID белого:", m_whiteId);
    form->addRow("ID чёрного:", m_blackId);
    form->addRow("Дата:", m_date);
    form->addRow("Время:", m_time);

    auto *layout = makeOwned<QVBoxLayout>(this);
    layout->addLayout(form);
    layout->addWidget(makeButtons(this));
}

void CreateMatchDialog::accept()
{
    tournamentId = m_tournamentId->value();
    whiteId = m_whiteId->value();
    blackId = m_blackId->value();
    date = m_date->date();
    time = m_time->time();

    QDialog::accept();
}

ResultDialog::ResultDialog(QWidget *parent)
    : QDialog(parent)
{
    setWindowTitle("Результат партии");

    m_result = makeOwned<QComboBox>(this);
    m_result->addItems({"Ничья", "Победа белых", "Победа чёрных"});

    auto *layout = makeOwned<QVBoxLayout>(this);
    layout->addWidget(m_result);
    auto *buttons = makeButtons(this);
    QObject::connect(buttons, &QDialogButtonBox::accepted, this, [this]() {
        result = m_result->currentIndex();
        QDialog::accept();
    });
    layout->addWidget(buttons);
}