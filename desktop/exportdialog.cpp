#include "exportdialog.h"
#include "ui_exportdialog.h"
#include "settings.h"
#include "datasource/webdatasource.h"
#include "datasource/databasedatasource.h"
#include "datasource/dialogprogresslistener.h"

#include <QFileDialog>
#include <QMessageBox>
#include <QProgressDialog>
#include <QDebug>
#include <cmath>

#define FILTERS "Data file (*.dat);;Comma separated values (*.csv);;Text file (*.txt)"

void disableCheckbox(QCheckBox *cb) {
    cb->setVisible(false);
    cb->setChecked(false);
}


ExportDialog::ExportDialog(bool solarDataAvailable, hardware_type_t hw_type,
                           QWidget *parent) :
    QDialog(parent),
    ui(new Ui::ExportDialog)
{
    ui->setupUi(this);

    if (hw_type != HW_DAVIS) {
        solarDataAvailable = false;
        disableCheckbox(ui->cbHighTemperature);
        disableCheckbox(ui->cbLowTemperature);
        disableCheckbox(ui->cbHighRainRate);
        disableCheckbox(ui->cbWirelessReception);
        disableCheckbox(ui->cbForecastRuleID);
        disableCheckbox(ui->cbGustWindDirection);
    }

    if (!solarDataAvailable) {
        disableCheckbox(ui->cbUVIndex);
        disableCheckbox(ui->cbSolarRadiation);
        disableCheckbox(ui->cbHighSolarRadiation);
        disableCheckbox(ui->cbHighUVIndex);
        disableCheckbox(ui->cbEvapotranspiration);
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
        dataSource.reset(new DatabaseDataSource(new DialogProgressListener(this), this));
    else
        dataSource.reset(new WebDataSource(new DialogProgressListener(this), this));

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
    if (ui->cbWirelessReception->isChecked() && ui->cbWirelessReception->isVisible())
        columns |= SC_Reception;
    if (ui->cbHighTemperature->isChecked() && ui->cbHighTemperature->isVisible())
        columns |= SC_HighTemperature;
    if (ui->cbLowTemperature->isChecked() && ui->cbLowTemperature->isVisible())
        columns |= SC_LowTemperature;
    if (ui->cbHighRainRate->isChecked() && ui->cbHighRainRate->isVisible())
        columns |= SC_HighRainRate;
    if (ui->cbGustWindDirection->isChecked() && ui->cbGustWindDirection->isVisible())
        columns |= SC_GustWindDirection;
    if (ui->cbEvapotranspiration->isChecked() && ui->cbEvapotranspiration->isVisible())
        columns |= SC_Evapotranspiration;
    if (ui->cbHighSolarRadiation->isChecked() && ui->cbHighSolarRadiation->isVisible())
        columns |= SC_HighSolarRadiation;
    if (ui->cbHighUVIndex->isChecked() && ui->cbHighUVIndex->isVisible())
        columns |= SC_HighUVIndex;
    if (ui->cbForecastRuleID->isChecked() && ui->cbForecastRuleID->isVisible())
        columns |= SC_ForecastRuleId;
    return columns;
}


void ExportDialog::exportData() {
    QString delimiter = getDelimiter();
    QDateTime startTime = getStartTime();
    QDateTime endTime = getEndTime();

    QStringList filters = QString(FILTERS).split(";;");
    QString selectedFilter = filters.first();
    dashNulls = ui->cbDashNulls->isChecked();

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

QString ExportDialog::dstr(double d, bool nodp) {
    if (std::isnan(d)) {
        if (dashNulls) {
            return "--";
        }
        return "";
    }

    if (nodp) {
        return QString::number(d);
    }

    return QString::number(d, 'f', 1);
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

    if (samples.reception.size() < samples.timestampUnix.size()) {
        // Reception not available in the data set (not valid for this station?
        columns = columns & ~SC_Reception;
    }

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
            rowData.append(dstr(samples.temperature.at(i)));
        if (columns.testFlag(SC_ApparentTemperature))
            rowData.append(dstr(samples.apparentTemperature.at(i)));
        if (columns.testFlag(SC_WindChill))
            rowData.append(dstr(samples.windChill.at(i)));
        if (columns.testFlag(SC_DewPoint))
            rowData.append(dstr(samples.dewPoint.at(i)));
        if (columns.testFlag(SC_Humidity))
            rowData.append(dstr(samples.humidity.at(i), true));
        if (columns.testFlag(SC_IndoorTemperature))
            rowData.append(dstr(samples.indoorTemperature.at(i)));
        if (columns.testFlag(SC_IndoorHumidity))
            rowData.append(dstr(samples.indoorHumidity.at(i), true));
        if (columns.testFlag(SC_Pressure))
            rowData.append(dstr(samples.pressure.at(i)));
        if (columns.testFlag(SC_Rainfall))
            rowData.append(dstr(samples.rainfall.at(i)));
        if (columns.testFlag(SC_AverageWindSpeed))
            rowData.append(dstr(samples.averageWindSpeed.at(i)));
        if (columns.testFlag(SC_WindDirection)) {
            time_t ts = samples.timestampUnix.at(i);
            if (samples.windDirection.contains(ts))
                rowData.append(QString::number(samples.windDirection[ts]));
             else
                 rowData.append("");
        }
        if (columns.testFlag(SC_GustWindSpeed))
            rowData.append(dstr(samples.gustWindSpeed.at(i)));
        if (columns.testFlag(SC_GustWindDirection)) {
            time_t ts = samples.timestampUnix.at(i);
            if (samples.gustWindDirection.contains(ts))
                rowData.append(QString::number(samples.gustWindDirection[ts]));
             else
                 rowData.append("");
        }
        if (columns.testFlag(SC_UV_Index))
            rowData.append(dstr(samples.uvIndex.at(i)));
        if (columns.testFlag(SC_SolarRadiation))
            rowData.append(dstr(samples.solarRadiation.at(i), true));
        if (columns.testFlag(SC_Evapotranspiration))
            rowData.append(dstr(samples.evapotranspiration.at(i), true));
        if (columns.testFlag(SC_HighTemperature))
            rowData.append(dstr(samples.highTemperature.at(i)));
        if (columns.testFlag(SC_LowTemperature))
            rowData.append(dstr(samples.lowTemperature.at(i)));
        if (columns.testFlag(SC_HighRainRate))
            rowData.append(dstr(samples.highRainRate.at(i)));
        if (columns.testFlag(SC_HighSolarRadiation))
            rowData.append(dstr(samples.highSolarRadiation.at(i)));
        if (columns.testFlag(SC_HighUVIndex))
            rowData.append(dstr(samples.highUVIndex.at(i)));
        if (columns.testFlag(SC_Reception))
            rowData.append(dstr(samples.reception.at(i)));
        if (columns.testFlag(SC_ForecastRuleId))
            rowData.append(QString::number(samples.forecastRuleId.at(i)));

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
    if (columns.testFlag(SC_WindDirection))
        columnNames.append("Wind Direction");
    if (columns.testFlag(SC_GustWindSpeed))
        columnNames.append("Gust Wind Speed");
    if (columns.testFlag(SC_GustWindDirection))
        columnNames.append("Gust Wind Direction");
    if (columns.testFlag(SC_UV_Index))
        columnNames.append("UV Index");
    if (columns.testFlag(SC_SolarRadiation))
        columnNames.append("Solar Radiation");
    if (columns.testFlag(SC_Evapotranspiration))
        columnNames.append("Evapotranspiration");
    if (columns.testFlag(SC_HighTemperature))
        columnNames.append("High Temperature");
    if (columns.testFlag(SC_LowTemperature))
        columnNames.append("Low Temperature");
    if (columns.testFlag(SC_HighRainRate))
        columnNames.append("High Rain Rate");
    if (columns.testFlag(SC_HighSolarRadiation))
        columnNames.append("High Solar Radiation");
    if (columns.testFlag(SC_HighUVIndex))
        columnNames.append("High UV Index");
    if (columns.testFlag(SC_Reception))
        columnNames.append("Wireless Reception");
    if (columns.testFlag(SC_ForecastRuleId))
        columnNames.append("Forecast Rule ID");

    return columnNames.join(getDelimiter()) + "\n";
}


