#include "exportdialog.h"
#include "ui_exportdialog.h"
#include "settings.h"
#include "datasource/webdatasource.h"
#include "datasource/databasedatasource.h"

#include <QFileDialog>
#include <QMessageBox>
#include <QProgressDialog>
#include <QDebug>

#define FILTERS "Data file (*.dat);;Comma separated values (*.csv);;Text file (*.txt)"



ExportDialog::ExportDialog(bool solarDataAvailable, QWidget *parent) :
    QDialog(parent),
    ui(new Ui::ExportDialog)
{
    ui->setupUi(this);

    if (!solarDataAvailable) {
        ui->cbUVIndex->setChecked(false);
        ui->cbUVIndex->setVisible(false);
        ui->cbSolarRadiation->setChecked(false);
        ui->cbSolarRadiation->setVisible(false);
    }

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
    return ui->timespan->getStartTime();
}

QDateTime ExportDialog::getEndTime() {
    return ui->timespan->getEndTime();
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
    if (ui->cbUVIndex->isChecked() && ui->cbUVIndex->isVisible())
        columns |= SC_UV_Index;
    if (ui->cbSolarRadiation->isChecked() && ui->cbSolarRadiation->isVisible())
        columns |= SC_SolarRadiation;
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
    qDebug() << "Export: samples ready.";
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
        dataFile.write(headerRow.toLatin1());

    QString delimiter = getDelimiter();

    qDebug() << "Generating delimited text file...";
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
        if (columns.testFlag(SC_UV_Index))
            rowData.append(QString::number(
                               samples.uvIndex.at(i), 'f', 1));
        if (columns.testFlag(SC_SolarRadiation))
            rowData.append(QString::number(
                               samples.solarRadiation.at(i)));

        QString row = rowData.join(delimiter) + "\n";
        dataFile.write(row.toLatin1());

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
    qDebug() << "Work complete.";
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

    if (!ui->cbIncludeHeadings->isChecked())
        return "";

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
    if (columns.testFlag(SC_UV_Index))
        columnNames.append("UV Index");
    if (columns.testFlag(SC_SolarRadiation))
        columnNames.append("Solar Radiation");

    return columnNames.join(getDelimiter()) + "\n";
}


