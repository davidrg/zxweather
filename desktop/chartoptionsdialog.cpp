#include "chartoptionsdialog.h"
#include "ui_chartoptionsdialog.h"

#include <QMessageBox>
#include <QtDebug>

ChartOptionsDialog::ChartOptionsDialog(QWidget *parent) :
    QDialog(parent),
    ui(new Ui::ChartOptionsDialog)
{
    ui->setupUi(this);

    // Chart types
    connect(ui->rbCtTemperature, SIGNAL(clicked()), this, SLOT(typeChanged()));
    connect(ui->rbCtHumidity, SIGNAL(clicked()), this, SLOT(typeChanged()));
    connect(ui->rbCtPressure, SIGNAL(clicked()), this, SLOT(typeChanged()));

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

void ChartOptionsDialog::typeChanged() {
    ui->cbApparentTemperature->setEnabled(false);
    ui->cbDewPoint->setEnabled(false);
    ui->cbHumidity->setEnabled(false);
    ui->cbIndoorHumidity->setEnabled(false);
    ui->cbIndoorTemperature->setEnabled(false);
    ui->cbTemperature->setEnabled(false);
    ui->cbWindChill->setEnabled(false);
    ui->cbPressure->setChecked(false);

    if (ui->rbCtTemperature->isChecked()) {
        ui->cbApparentTemperature->setEnabled(true);
        ui->cbDewPoint->setEnabled(true);
        ui->cbIndoorTemperature->setEnabled(true);
        ui->cbTemperature->setEnabled(true);
        ui->cbWindChill->setEnabled(true);
    } else if (ui->rbCtHumidity->isChecked()) {
        ui->cbHumidity->setEnabled(true);
        ui->cbIndoorHumidity->setEnabled(true);
    } else if (ui->rbCtPressure->isChecked()) {
        // Don't bother enabling pressure - it always has to be checked if
        // we're plotting pressure.
        ui->cbPressure->setChecked(true);
    }

}

void ChartOptionsDialog::checkAndAccept() {
    columns.clear();

    if (ui->rbCtTemperature->isChecked()) {
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
    } else if (ui->rbCtHumidity->isChecked()) {
        if (ui->cbHumidity->isChecked())
            columns.append(COL_HUMIDITY);
        if (ui->cbIndoorHumidity->isChecked())
            columns.append(COL_HUMIDITY_INDOORS);
    } else if (ui->rbCtPressure->isChecked()) {
        columns.append(COL_PRESSURE);
    }

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
        qDebug() << time;
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

enum ChartOptionsDialog::ChartType ChartOptionsDialog::getChartType() {
    if (ui->rbCtTemperature->isChecked()) {
        return ChartOptionsDialog::Temperature;
    } else if (ui->rbCtHumidity->isChecked()) {
        return ChartOptionsDialog::Humidity;
    }

    return ChartOptionsDialog::Pressure;
}
