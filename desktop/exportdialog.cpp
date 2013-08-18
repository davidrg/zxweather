#include "exportdialog.h"
#include "ui_exportdialog.h"
#include "settings.h"
#include "datasource/webdatasource.h"
#include "datasource/databasedatasource.h"

#include <QFileDialog>
#include <QMessageBox>
#include <QProgressDialog>

#define FILTERS "Data file (*.dat);;Comma separated values (*.csv);;Text file (*.txt)"



ExportDialog::ExportDialog(QWidget *parent) :
    QDialog(parent),
    ui(new Ui::ExportDialog)
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

    // Delimiter types
    connect(ui->rbCommaDelimited, SIGNAL(clicked()),
            this, SLOT(delimiterTypeChanged()));
    connect(ui->rbTabDelimited, SIGNAL(clicked()),
            this, SLOT(delimiterTypeChanged()));
    connect(ui->rbOtherDelimiter, SIGNAL(clicked()),
            this, SLOT(delimiterTypeChanged()));

    // ok button
    connect(ui->buttonBox, SIGNAL(accepted()), this, SLOT(exportData()));

    // Setup data source
    Settings& settings = Settings::getInstance();

    if (settings.sampleDataSourceType() == Settings::DS_TYPE_DATABASE)
        dataSource.reset(new DatabaseDataSource(this, this));
    else
        dataSource.reset(new WebDataSource(this, this));

    connect(dataSource.data(), SIGNAL(samplesReady(SampleSet)),
            this, SLOT(samplesReady(SampleSet)));
    connect(dataSource.data(), SIGNAL(sampleRetrievalError(QString)),
            this, SLOT(samplesFailed(QString)));
}

ExportDialog::~ExportDialog()
{
    delete ui;
}

void ExportDialog::dateChanged() {
    if (ui->rbTCustom->isChecked()) {
        ui->startTime->setEnabled(true);
        ui->endTime->setEnabled(true);
    } else {
        ui->startTime->setEnabled(false);
        ui->endTime->setEnabled(false);
    }
}

void ExportDialog::delimiterTypeChanged() {
    if (ui->rbOtherDelimiter->isChecked())
        ui->leCustomDelimiter->setEnabled(true);
    else
        ui->leCustomDelimiter->setEnabled(false);
}

QString ExportDialog::getDelimiter() {
    QString delimiter;
    if (ui->rbCommaDelimited->isChecked())
        delimiter = ",";
    else if (ui->rbTabDelimited->isChecked())
        delimiter = "\t";
    else if (ui->rbOtherDelimiter->isChecked())
        delimiter = ui->leCustomDelimiter->text();

    return delimiter;
}

QDateTime ExportDialog::getStartTime() {
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

QDateTime ExportDialog::getEndTime() {
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

SampleColumns ExportDialog::getColumns()
{
    SampleColumns columns;


    if (ui->cbTimestamp->isChecked())
        columns |= SC_Timestamp;
    if (ui->cbApparentTemperature->isChecked())
        columns |= SC_ApparentTemperature;
    if (ui->cbDewPoint->isChecked())
        columns |= SC_DewPoint;
    if (ui->cbHumidity->isChecked())
        columns |= SC_Humidity;
    if (ui->cbIndoorHumidity->isChecked())
        columns |= SC_IndoorHumidity;
    if (ui->cbIndoorTemperature->isChecked())
        columns |= SC_IndoorTemperature;
    if (ui->cbPressure->isChecked())
        columns |= SC_Pressure;
    if (ui->cbRainfall->isChecked())
        columns |= SC_Rainfall;
    if (ui->cbTemperature->isChecked())
        columns |= SC_Temperature;
    if (ui->cbWindChill->isChecked())
        columns |= SC_WindChill;
    if (ui->cbAverageWindSpeed->isChecked())
        columns |= SC_AverageWindSpeed;
    if (ui->cbGustWindSpeed->isChecked())
        columns |= SC_GustWindSpeed;
    if (ui->cbWindDirection->isChecked())
        columns |= SC_WindDirection;

    return columns;
}


void ExportDialog::exportData() {
    QString delimiter = getDelimiter();
    QDateTime startTime = getStartTime();
    QDateTime endTime = getEndTime();

    QStringList filters = QString(FILTERS).split(";;");
    QString selectedFilter = filters.first();

    if (delimiter == ",")
        selectedFilter = filters.at(1); // csv

    QString filename = QFileDialog::getSaveFileName(this,
                                                    "Export data...",
                                                    QString(),
                                                    FILTERS,
                                                    &selectedFilter);
    if (filename.isEmpty()) {
        reject(); // user canceled
        return;
    }

    targetFilename = filename;

    dataSource->fetchSamples(getColumns(), startTime, endTime);
}

void ExportDialog::samplesReady(SampleSet samples)
{
    int sampleCount = samples.timestamp.count();
    QProgressDialog progressDialog(this);
    progressDialog.setWindowTitle("Exporting Data...");
    progressDialog.setMaximum(sampleCount);

    QFile dataFile(targetFilename);

    if (!dataFile.open(QIODevice::WriteOnly)) {
        QMessageBox::critical(this, "Error opening file",
                              "Failed to open file for writing. " +
                              dataFile.errorString());
        reject();
        return;
    }

    SampleColumns columns = getColumns();

    QString headerRow = getHeaderRow(columns);
    if (!headerRow.isEmpty())
        dataFile.write(headerRow.toAscii());

    QString delimiter = getDelimiter();

    for (int i = 0; i < samples.timestamp.count(); i++) {
        QStringList rowData;

        if (columns.testFlag(SC_Timestamp))
            rowData.append(QDateTime::fromTime_t(
                               samples.timestampUnix.at(i))
                           .toString(Qt::ISODate));
        if (columns.testFlag(SC_Temperature))
            rowData.append(QString::number(samples.temperature.at(i),'f', 1));
        if (columns.testFlag(SC_ApparentTemperature))
            rowData.append(QString::number(
                               samples.apparentTemperature.at(i),'f', 1));
        if (columns.testFlag(SC_WindChill))
            rowData.append(QString::number(samples.windChill.at(i),'f', 1));
        if (columns.testFlag(SC_DewPoint))
            rowData.append(QString::number(samples.dewPoint.at(i),'f', 1));
        if (columns.testFlag(SC_Humidity))
            rowData.append(QString::number(samples.humidity.at(i)));
        if (columns.testFlag(SC_IndoorTemperature))
            rowData.append(QString::number(
                               samples.indoorTemperature.at(i),'f', 1));
        if (columns.testFlag(SC_IndoorHumidity))
            rowData.append(QString::number(samples.indoorHumidity.at(i)));
        if (columns.testFlag(SC_Pressure))
            rowData.append(QString::number(samples.pressure.at(i),'f', 1));
        if (columns.testFlag(SC_Rainfall))
            rowData.append(QString::number(samples.rainfall.at(i),'f', 1));
        if (columns.testFlag(SC_AverageWindSpeed))
            rowData.append(QString::number(
                               samples.averageWindSpeed.at(i),'f',1));
        if (columns.testFlag(SC_GustWindSpeed))
            rowData.append(QString::number(samples.gustWindSpeed.at(i),'f',1));
        if (columns.testFlag(SC_WindDirection)) {
            time_t ts = samples.timestampUnix.at(i);
            if (samples.windDirection.contains(ts))
                rowData.append(QString::number(samples.windDirection[ts]));
             else
                 rowData.append("");
        }

        QString row = rowData.join(delimiter) + "\n";
        dataFile.write(row.toAscii());

        // Only update on every 25th row
        if (i % 25 == 0) {
            progressDialog.setValue(i);

            if (progressDialog.wasCanceled()) {
                dataFile.close();
                reject();
                return;
            }
        }

    }
    dataFile.close();
    progressDialog.reset();
    accept();
}

void ExportDialog::samplesFailed(QString message)
{
    QMessageBox::critical(this, "Error", message);
    reject();
}

QString ExportDialog::getHeaderRow(SampleColumns columns) {
    QStringList columnNames;

    if (columns.testFlag(SC_Timestamp))
        columnNames.append("Timestamp");
    if (columns.testFlag(SC_Temperature))
        columnNames.append("Temperature");
    if (columns.testFlag(SC_ApparentTemperature))
        columnNames.append("Apparent Temperature");
    if (columns.testFlag(SC_WindChill))
        columnNames.append("Wind Chill");
    if (columns.testFlag(SC_DewPoint))
        columnNames.append("Dew Point");
    if (columns.testFlag(SC_Humidity))
        columnNames.append("Humidity");
    if (columns.testFlag(SC_IndoorTemperature))
        columnNames.append("Indoor Temperature");
    if (columns.testFlag(SC_IndoorHumidity))
        columnNames.append("Indoor Humidity");
    if (columns.testFlag(SC_Pressure))
         columnNames.append("Pressure");
    if (columns.testFlag(SC_Rainfall))
        columnNames.append("Rainfall");
    if (columns.testFlag(SC_AverageWindSpeed))
        columnNames.append("Average Wind Speed");
    if (columns.testFlag(SC_GustWindSpeed))
        columnNames.append("Gust Wind Speed");
    if (columns.testFlag(SC_WindDirection))
        columnNames.append("Wind Direction");

    return columnNames.join(getDelimiter()) + "\n";
}


