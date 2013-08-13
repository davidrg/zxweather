#include "chartoptionsdialog.h"
#include "ui_chartoptionsdialog.h"

#include <QMessageBox>
#include <QtDebug>

ChartOptionsDialog::ChartOptionsDialog(QWidget *parent) :
    QDialog(parent),
    ui(new Ui::ChartOptionsDialog)
{
    ui->setupUi(this);

    // Time ranges
    connect(ui->rbTCustom, SIGNAL(clicked()), this, SLOT(dateChanged()));
    connect(ui->rbTThisMonth, SIGNAL(clicked()), this, SLOT(dateChanged()));
    connect(ui->rbTThisWeek, SIGNAL(clicked()), this, SLOT(dateChanged()));
    connect(ui->rbTThisYear, SIGNAL(clicked()), this, SLOT(dateChanged()));
    connect(ui->rbTToday, SIGNAL(clicked()), this, SLOT(dateChanged()));
    connect(ui->rbTYesterday, SIGNAL(clicked()), this, SLOT(dateChanged()));

    // Buttons
    connect(this->ui->buttonBox, SIGNAL(accepted()), this, SLOT(checkAndAccept()));
}

ChartOptionsDialog::~ChartOptionsDialog()
{
    delete ui;
}

void ChartOptionsDialog::checkAndAccept() {
    columns.clear();

    if (ui->cbTemperature->isChecked())
        columns.append(COL_TEMPERATURE);
    if (ui->cbIndoorTemperature->isChecked())
        columns.append(COL_TEMPERATURE_INDOORS);
    if (ui->cbApparentTemperature->isChecked())
        columns.append(COL_APPARENT_TEMPERATURE);
    if (ui->cbDewPoint->isChecked())
        columns.append(COL_DEW_POINT);
    if (ui->cbWindChill->isChecked())
        columns.append(COL_WIND_CHILL);
    if (ui->cbHumidity->isChecked())
        columns.append(COL_HUMIDITY);
    if (ui->cbIndoorHumidity->isChecked())
        columns.append(COL_HUMIDITY_INDOORS);
    if (ui->cbPressure->isChecked())
        columns.append(COL_PRESSURE);
    if (ui->cbRainfall->isChecked())
        columns.append(COL_RAINFALL);
    if (ui->cbAverageWindSpeed->isChecked())
        columns.append(COL_AVG_WINDSPEED);
    if (ui->cbGustWindSpeed->isChecked())
        columns.append(COL_GUST_WINDSPEED);
    if (ui->cbWindDirection->isChecked())
        columns.append(COL_WIND_DIRECTION);

    if (columns.isEmpty()) {
        QMessageBox::information(this, "Data Sets",
                                 "At least one data set must be selected");
        return;
    }

    accept();
}

void ChartOptionsDialog::dateChanged() {
    if (ui->rbTCustom->isChecked()) {
        ui->startTime->setEnabled(true);
        ui->endTime->setEnabled(true);
    } else {
        ui->startTime->setEnabled(false);
        ui->endTime->setEnabled(false);
    }
}

QDateTime ChartOptionsDialog::getStartTime() {
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

QDateTime ChartOptionsDialog::getEndTime() {
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

QList<int> ChartOptionsDialog::getColumns() {
    return columns;
}

