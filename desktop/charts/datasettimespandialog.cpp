#include "datasettimespandialog.h"
#include "ui_datasettimespandialog.h"

DataSetTimespanDialog::DataSetTimespanDialog(QWidget *parent) :
    QDialog(parent),
    ui(new Ui::DataSetTimespanDialog)
{
    ui->setupUi(this);

    ui->startTime->setDateTime(QDateTime::currentDateTime().addDays(-1));
    ui->endTime->setDateTime(QDateTime::currentDateTime());

    // Time ranges
    connect(ui->rbTCustom, SIGNAL(clicked()), this, SLOT(dateChanged()));
    connect(ui->rbTThisMonth, SIGNAL(clicked()), this, SLOT(dateChanged()));
    connect(ui->rbTThisWeek, SIGNAL(clicked()), this, SLOT(dateChanged()));
    connect(ui->rbTThisYear, SIGNAL(clicked()), this, SLOT(dateChanged()));
    connect(ui->rbTToday, SIGNAL(clicked()), this, SLOT(dateChanged()));
    connect(ui->rbTYesterday, SIGNAL(clicked()), this, SLOT(dateChanged()));

}

DataSetTimespanDialog::~DataSetTimespanDialog()
{
    delete ui;
}

void DataSetTimespanDialog::dateChanged() {
    if (ui->rbTCustom->isChecked()) {
        ui->startTime->setEnabled(true);
        ui->endTime->setEnabled(true);
    } else {
        ui->startTime->setEnabled(false);
        ui->endTime->setEnabled(false);
    }
}


QDateTime DataSetTimespanDialog::getStartTime() {
    QDateTime time = QDateTime::currentDateTime();
    time.setTime(QTime(0,0));

    int year = time.date().year();
    int month = time.date().month();

    if (ui->rbTToday->isChecked())
        return time;
    else if (ui->rbTYesterday->isChecked()) {
        time.setDate(time.date().addDays(-1));
        return time;
    } else if (ui->rbTThisWeek->isChecked()) {
        int subtractDays = 1 - time.date().dayOfWeek();
        time.setDate(time.date().addDays(subtractDays));
        return time;
    } else if (ui->rbTThisMonth->isChecked()) {
        time.setDate(QDate(year, month, 1));
        return time;
    } else if (ui->rbTThisYear->isChecked()) {
        time.setDate(QDate(year,1,1));
        return time;
    } else
        return ui->startTime->dateTime();
}

QDateTime DataSetTimespanDialog::getEndTime() {
    QDateTime time = QDateTime::currentDateTime();
    time.setTime(QTime(23,59,59));

    int year = time.date().year();
    int month = time.date().month();

    if (ui->rbTToday->isChecked()) {
        return time;
    } else if (ui->rbTYesterday->isChecked()) {
        time.setDate(time.date().addDays(-1));
        return time;
    } else if (ui->rbTThisWeek->isChecked()) {
        int addDays = 7 - time.date().dayOfWeek();
        time.setDate(time.date().addDays(addDays));
        return time;
    } else if (ui->rbTThisMonth->isChecked()) {
        QDate date(year,month,1);
        date = date.addMonths(1);
        date = date.addDays(-1);
        time.setDate(date);
        return time;
    } else if (ui->rbTThisYear->isChecked()) {
        time.setDate(QDate(year,12,31));
        return time;
    } else
        return ui->endTime->dateTime();
}

void DataSetTimespanDialog::setTime(QDateTime start, QDateTime end) {
    // Work out start times for radio buttons
    QDateTime time = QDateTime::currentDateTime();
    time.setTime(QTime(0,0));
    int year = time.date().year();
    int month = time.date().month();

    QDateTime todayStart = time;
    QDateTime yesterdayStart = time.addDays(-1);

    time = QDateTime::currentDateTime();
     time.setTime(QTime(0,0));
    int subtractDays = 1 - time.date().dayOfWeek();
    QDateTime thisWeekStart = time.addDays(subtractDays);

    time.setDate(QDate(year, month, 1));
    QDateTime thisMonthStart = time;

    time.setDate(QDate(year,1,1));
    QDateTime thisYearStart = time;

    // Work out end times for radio buttons

    time = QDateTime::currentDateTime();
    time.setTime(QTime(23,59,59));

    QDateTime todayEnd = time;
    QDateTime yesterdayEnd = time.addDays(-1);

    time = QDateTime::currentDateTime();
    time.setTime(QTime(23,59,59));
    int addDays = 7 - time.date().dayOfWeek();
    QDateTime thisWeekEnd = time.addDays(addDays);

    time = QDateTime::currentDateTime();
    time.setTime(QTime(23,59,59));
    QDate date(year,month,1);
    date = date.addMonths(1);
    date = date.addDays(-1);
    time.setDate(date);
    QDateTime thisMonthEnd = time;

    time.setTime(QTime(23,59,59));
    time.setDate(QDate(year, 12, 31));
    QDateTime thisYearEnd = time;

    if (start == todayStart && end == todayEnd) {
        ui->rbTToday->setChecked(true);
    } else if (start == yesterdayStart && end == yesterdayEnd) {
        ui->rbTYesterday->setChecked(true);
    } else if (start == thisWeekStart && end == thisWeekEnd) {
        ui->rbTThisWeek->setChecked(true);
    } else if (start == thisMonthStart && end == thisMonthEnd) {
        ui->rbTThisMonth->setChecked(true);
    } else if (start == thisYearStart && end == thisYearEnd) {
        ui->rbTThisYear->setChecked(true);
    } else {
        ui->rbTCustom->setChecked(true);
    }

    ui->startTime->setDateTime(start);
    ui->endTime->setDateTime(end);
}
