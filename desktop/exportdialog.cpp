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


ExportDialog::ExportDialog(bool solarDataAvailable, bool isWireless, hardware_type_t hw_type,
                           QWidget *parent) :
    QDialog(parent),
    ui(new Ui::ExportDialog)
{
    ui->setupUi(this);

    if (Settings::getInstance().imperial()) {
        ui->cbUnits->setChecked(true);
    } else {
        // Metric!
        ui->cbUnits->setText(tr("Export wind speed in km/h"));
        ui->cbUnits->setChecked(Settings::getInstance().kmh());
    }

    if (hw_type != HW_DAVIS) {
        solarDataAvailable = false;
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

    ui->columnPicker->configure(
                solarDataAvailable, hw_type, isWireless,
                dataSource->extraColumnsAvailable(),
                dataSource->extraColumnNames(), true);

    connect(ui->pbSelectAll, SIGNAL(clicked(bool)), ui->columnPicker, SLOT(checkAll()));
    connect(ui->pbSelectNone, SIGNAL(clicked(bool)), ui->columnPicker, SLOT(uncheckAll()));

    ui->columnPicker->checkAll();
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
                                                    tr("Export data..."),
                                                    QString(),
                                                    FILTERS,
                                                    &selectedFilter);
    if (filename.isEmpty()) {
        reject(); // user canceled
        return;
    }

    targetFilename = filename;

    dataSource->fetchSamples(ui->columnPicker->getColumns(), startTime, endTime);
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
    progressDialog.setWindowTitle(tr("Exporting Data..."));
    progressDialog.setMaximum(sampleCount);

    QFile dataFile(targetFilename);

    if (!dataFile.open(QIODevice::WriteOnly)) {
        QMessageBox::critical(this, tr("Error opening file"),
                              QString(tr("Failed to open file for writing. %1")).arg(dataFile.errorString()));
        reject();
        return;
    }

    QTextStream streamDataFile(&dataFile);
    streamDataFile.setCodec("UTF-8");

    SampleColumns columns = ui->columnPicker->getColumns();
    columns.standard |= SC_Timestamp;

    if (samples.reception.size() < samples.timestampUnix.size()) {
        // Reception not available in the data set (not valid for this station?
        columns.standard = columns.standard & ~SC_Reception;
    }

    QString headerRow = getHeaderRow(columns);
    if (!headerRow.isEmpty())
        streamDataFile << headerRow;

    QString delimiter = getDelimiter();

    bool convertUnits = ui->cbUnits->isChecked();
    bool imperial = convertUnits ? Settings::getInstance().imperial() : false;
    bool kmh = imperial ? false : Settings::getInstance().kmh();

    using namespace UnitConversions;

    bool isoTime = ui->cbISOTime->isChecked();

    qDebug() << "Generating delimited text file...";
    for (int i = 0; i < samples.timestamp.count(); i++) {
        QStringList rowData;

        if (columns.standard.testFlag(SC_Timestamp)) {
            if (isoTime) {
                rowData.append(QDateTime::fromTime_t(
                                   samples.timestampUnix.at(i))
                               .toString(Qt::ISODate));
            } else {
                rowData.append(QDateTime::fromTime_t(
                                   samples.timestampUnix.at(i))
                               .toString("yyyy-MM-dd HH:mm:ss"));
            }
        } if (columns.standard.testFlag(SC_Temperature)) {
            double value = samples.temperature.at(i);
            if (imperial) value = celsiusToFahrenheit(value);
            rowData.append(dstr(value));
        } if (columns.standard.testFlag(SC_ApparentTemperature)) {
            double value = samples.apparentTemperature.at(i);
            if (imperial) value = celsiusToFahrenheit(value);
            rowData.append(dstr(value));
        } if (columns.standard.testFlag(SC_WindChill)) {
            double value = samples.windChill.at(i);
            if (imperial) value = celsiusToFahrenheit(value);
            rowData.append(dstr(value));
        } if (columns.standard.testFlag(SC_DewPoint)) {
            double value = samples.dewPoint.at(i);
            if (imperial) value = celsiusToFahrenheit(value);
            rowData.append(dstr(value));
        } if (columns.standard.testFlag(SC_Humidity)) {
            rowData.append(dstr(samples.humidity.at(i), true));
        } if (columns.standard.testFlag(SC_IndoorTemperature)) {
            double value = samples.indoorTemperature.at(i);
            if (imperial) value = celsiusToFahrenheit(value);
            rowData.append(dstr(value));
        } if (columns.standard.testFlag(SC_IndoorHumidity)) {
            rowData.append(dstr(samples.indoorHumidity.at(i), true));
        } if (columns.standard.testFlag(SC_Pressure)) {
            double value = samples.pressure.at(i);
            if (imperial) value = hectopascalsToInchesOfMercury(value);
            rowData.append(dstr(value));
        } if (columns.standard.testFlag(SC_Rainfall)) {
            double value = samples.rainfall.at(i);
            if (imperial) value = millimetersToInches(value);
            rowData.append(dstr(value));
        } if (columns.standard.testFlag(SC_AverageWindSpeed)) {
            double value = samples.averageWindSpeed.at(i);
            if (imperial) value = metersPerSecondToMilesPerHour(value);
            else if (kmh) value = metersPerSecondToKilometersPerHour(value);
            rowData.append(dstr(value));
        } if (columns.standard.testFlag(SC_WindDirection)) {
            time_t ts = samples.timestampUnix.at(i);
            if (samples.windDirection.contains(ts))
                rowData.append(QString::number(samples.windDirection[ts]));
             else
                 rowData.append("");
        } if (columns.standard.testFlag(SC_GustWindSpeed)) {
            double value = samples.gustWindSpeed.at(i);
            if (imperial) value = metersPerSecondToMilesPerHour(value);
            else if (kmh) value = metersPerSecondToKilometersPerHour(value);
            rowData.append(dstr(value));
        } if (columns.standard.testFlag(SC_GustWindDirection)) {
            time_t ts = samples.timestampUnix.at(i);
            if (samples.gustWindDirection.contains(ts))
                rowData.append(QString::number(samples.gustWindDirection[ts]));
             else
                 rowData.append("");
        } if (columns.standard.testFlag(SC_UV_Index)) {
            rowData.append(dstr(samples.uvIndex.at(i)));
        } if (columns.standard.testFlag(SC_SolarRadiation)) {
            rowData.append(dstr(samples.solarRadiation.at(i), true));
        } if (columns.standard.testFlag(SC_Evapotranspiration)) {
            double value = samples.evapotranspiration.at(i);
            if (imperial) value = millimetersToInches(value);
            rowData.append(dstr(value));
        } if (columns.standard.testFlag(SC_HighTemperature)) {
            double value = samples.highTemperature.at(i);
            if (imperial) value = celsiusToFahrenheit(value);
            rowData.append(dstr(value));
        } if (columns.standard.testFlag(SC_LowTemperature)) {
            double value = samples.lowTemperature.at(i);
            if (imperial) value = celsiusToFahrenheit(value);
            rowData.append(dstr(value));
        } if (columns.standard.testFlag(SC_HighRainRate)) {
            double value = samples.highRainRate.at(i);
            if (imperial) value = millimetersToInches(value);
            rowData.append(dstr(value));
        } if (columns.standard.testFlag(SC_HighSolarRadiation)) {
            rowData.append(dstr(samples.highSolarRadiation.at(i)));
        } if (columns.standard.testFlag(SC_HighUVIndex)) {
            rowData.append(dstr(samples.highUVIndex.at(i)));
        } if (columns.standard.testFlag(SC_Reception)) {
            rowData.append(dstr(samples.reception.at(i)));
        } if (columns.standard.testFlag(SC_ForecastRuleId)) {
            rowData.append(QString::number(samples.forecastRuleId.at(i)));
        } if (columns.extra.testFlag(EC_SoilMoisture1)) {
            rowData.append(dstr(samples.soilMoisture1.at(i)));
        } if (columns.extra.testFlag(EC_SoilMoisture2)) {
            rowData.append(dstr(samples.soilMoisture2.at(i)));
        } if (columns.extra.testFlag(EC_SoilMoisture3)) {
            rowData.append(dstr(samples.soilMoisture3.at(i)));
        } if (columns.extra.testFlag(EC_SoilMoisture4)) {
            rowData.append(dstr(samples.soilMoisture4.at(i)));
        } if (columns.extra.testFlag(EC_SoilTemperature1)) {
            rowData.append(dstr(samples.soilTemperature1.at(i)));
        } if (columns.extra.testFlag(EC_SoilTemperature2)) {
            rowData.append(dstr(samples.soilTemperature2.at(i)));
        } if (columns.extra.testFlag(EC_SoilTemperature3)) {
            rowData.append(dstr(samples.soilTemperature3.at(i)));
        } if (columns.extra.testFlag(EC_SoilTemperature4)) {
            rowData.append(dstr(samples.soilTemperature4.at(i)));
        } if (columns.extra.testFlag(EC_LeafWetness1)) {
            rowData.append(dstr(samples.leafWetness1.at(i), true));
        } if (columns.extra.testFlag(EC_LeafWetness2)) {
            rowData.append(dstr(samples.leafWetness2.at(i), true));
        } if (columns.extra.testFlag(EC_LeafTemperature1)) {
            rowData.append(dstr(samples.leafTemperature1.at(i)));
        } if (columns.extra.testFlag(EC_LeafTemperature2)) {
            rowData.append(dstr(samples.leafTemperature2.at(i)));
        } if (columns.extra.testFlag(EC_ExtraTemperature1)) {
            rowData.append(dstr(samples.extraTemperature1.at(i)));
        } if (columns.extra.testFlag(EC_ExtraTemperature2)) {
            rowData.append(dstr(samples.extraTemperature2.at(i)));
        } if (columns.extra.testFlag(EC_ExtraTemperature3)) {
            rowData.append(dstr(samples.extraTemperature3.at(i)));
        } if (columns.extra.testFlag(EC_ExtraHumidity1)) {
            rowData.append(dstr(samples.extraHumidity1.at(i)));
        } if (columns.extra.testFlag(EC_ExtraHumidity2)) {
            rowData.append(dstr(samples.extraHumidity2.at(i)));
        }

        QString row = rowData.join(delimiter) + "\n";
        streamDataFile << row;

        // Only update on every 25th row
        if (i % 25 == 0) {
            progressDialog.setValue(i);

            if (progressDialog.wasCanceled()) {
                streamDataFile.flush();
                dataFile.close();
                reject();
                return;
            }
        }

    }
    qDebug() << "Work complete.";
    streamDataFile.flush();
    dataFile.close();
    progressDialog.reset();
    accept();
}

void ExportDialog::samplesFailed(QString message)
{
    QMessageBox::critical(this, tr("Error"), message);
    reject();
}

QString ExportDialog::getHeaderRow(SampleColumns columns) {

    if (!ui->cbIncludeHeadings->isChecked())
        return "";

    using namespace UnitConversions;
    bool imperial = ui->cbUnits->isChecked() && Settings::getInstance().imperial();
    bool kmh = !imperial && ui->cbUnits->isChecked() && Settings::getInstance().kmh();

    QString temp = imperial ? unitString(U_FAHRENHEIT): unitString(U_CELSIUS);
    QString windSpeed = imperial ? unitString(U_MILES_PER_HOUR) : kmh ? unitString(U_KILOMETERS_PER_HOUR) : unitString(U_METERS_PER_SECOND);

    QStringList columnNames;

    if (columns.standard.testFlag(SC_Timestamp))
        columnNames.append(tr("Timestamp"));
    if (columns.standard.testFlag(SC_Temperature))
        columnNames.append(tr("Temperature (%1)").arg(temp));
    if (columns.standard.testFlag(SC_ApparentTemperature))
        columnNames.append(tr("Apparent Temperature (%1)").arg(temp));
    if (columns.standard.testFlag(SC_WindChill))
        columnNames.append(tr("Wind Chill (%1)").arg(temp));
    if (columns.standard.testFlag(SC_DewPoint))
        columnNames.append(tr("Dew Point (%1)").arg(temp));
    if (columns.standard.testFlag(SC_Humidity))
        columnNames.append(tr("Humidity (%)"));
    if (columns.standard.testFlag(SC_IndoorTemperature))
        columnNames.append(tr("Indoor Temperature (%1)").arg(temp));
    if (columns.standard.testFlag(SC_IndoorHumidity))
        columnNames.append(tr("Indoor Humidity (%)"));
    if (columns.standard.testFlag(SC_Pressure))
         columnNames.append(tr("Pressure (%1)").arg(imperial ? unitString(U_INCHES_OF_MERCURY) : unitString(U_HECTOPASCALS)));
    if (columns.standard.testFlag(SC_Rainfall))
        columnNames.append(tr("Rainfall (%1)").arg(imperial ? unitString(U_INCHES) : unitString(U_MILLIMETERS)));
    if (columns.standard.testFlag(SC_AverageWindSpeed))
        columnNames.append(tr("Average Wind Speed (%1)").arg(windSpeed));
    if (columns.standard.testFlag(SC_WindDirection))
        columnNames.append(tr("Wind Direction (%1)").arg(unitString(U_DEGREES)));
    if (columns.standard.testFlag(SC_GustWindSpeed))
        columnNames.append(tr("Gust Wind Speed (%1)").arg(windSpeed));
    if (columns.standard.testFlag(SC_GustWindDirection))
        columnNames.append(tr("Gust Wind Direction (%1)").arg(unitString(U_DEGREES)));
    if (columns.standard.testFlag(SC_UV_Index))
        columnNames.append(tr("UV Index"));
    if (columns.standard.testFlag(SC_SolarRadiation))
        columnNames.append(tr("Solar Radiation (%1)").arg(unitString(U_WATTS_PER_SQUARE_METER)));
    if (columns.standard.testFlag(SC_Evapotranspiration))
        columnNames.append(tr("Evapotranspiration (%1)").arg(imperial ? unitString(U_INCHES) : unitString(U_MILLIMETERS)));
    if (columns.standard.testFlag(SC_HighTemperature))
        columnNames.append(tr("High Temperature (%1)").arg(temp));
    if (columns.standard.testFlag(SC_LowTemperature))
        columnNames.append(tr("Low Temperature (%1)").arg(temp));
    if (columns.standard.testFlag(SC_HighRainRate))
        columnNames.append(tr("High Rain Rate (%1)").arg(imperial ? unitString(U_INCHES_PER_HOUR) : unitString(U_MILLIMETERS_PER_HOUR)));
    if (columns.standard.testFlag(SC_HighSolarRadiation))
        columnNames.append(tr("High Solar Radiation (%1)").arg(unitString(U_WATTS_PER_SQUARE_METER)));
    if (columns.standard.testFlag(SC_HighUVIndex))
        columnNames.append(tr("High UV Index"));
    if (columns.standard.testFlag(SC_Reception))
        columnNames.append(tr("Wireless Reception (%)"));
    if (columns.standard.testFlag(SC_ForecastRuleId))
        columnNames.append(tr("Forecast Rule ID"));

    QMap<ExtraColumn, QString> extraColumnNames = dataSource->extraColumnNames();

    QString cbar = unitString(U_CENTIBAR);

    if (columns.extra.testFlag(EC_SoilMoisture1)) {
        columnNames.append(extraColumnNames[EC_SoilMoisture1] + QString(" (%1)").arg(cbar));
    } if (columns.extra.testFlag(EC_SoilMoisture2)) {
        columnNames.append(extraColumnNames[EC_SoilMoisture2] + QString(" (%1)").arg(cbar));
    } if (columns.extra.testFlag(EC_SoilMoisture3)) {
        columnNames.append(extraColumnNames[EC_SoilMoisture3] + QString(" (%1)").arg(cbar));
    } if (columns.extra.testFlag(EC_SoilMoisture4)) {
        columnNames.append(extraColumnNames[EC_SoilMoisture4] + QString(" (%1)").arg(cbar));
    } if (columns.extra.testFlag(EC_SoilTemperature1)) {
        columnNames.append(extraColumnNames[EC_SoilTemperature1] + QString(" (%1)").arg(temp));
    } if (columns.extra.testFlag(EC_SoilTemperature2)) {
        columnNames.append(extraColumnNames[EC_SoilTemperature2] + QString(" (%1)").arg(temp));
    } if (columns.extra.testFlag(EC_SoilTemperature3)) {
        columnNames.append(extraColumnNames[EC_SoilTemperature3] + QString(" (%1)").arg(temp));
    } if (columns.extra.testFlag(EC_SoilTemperature4)) {
        columnNames.append(extraColumnNames[EC_SoilTemperature4] + QString(" (%1)").arg(temp));
    } if (columns.extra.testFlag(EC_LeafWetness1)) {
        columnNames.append(extraColumnNames[EC_LeafWetness1]);
    } if (columns.extra.testFlag(EC_LeafWetness2)) {
        columnNames.append(extraColumnNames[EC_LeafWetness2]);
    } if (columns.extra.testFlag(EC_LeafTemperature1)) {
        columnNames.append(extraColumnNames[EC_LeafTemperature1] + QString(" (%1)").arg(temp));
    } if (columns.extra.testFlag(EC_LeafTemperature2)) {
        columnNames.append(extraColumnNames[EC_LeafTemperature2] + QString(" (%1)").arg(temp));
    } if (columns.extra.testFlag(EC_ExtraTemperature1)) {
        columnNames.append(extraColumnNames[EC_ExtraTemperature1] + QString(" (%1)").arg(temp));
    } if (columns.extra.testFlag(EC_ExtraTemperature2)) {
        columnNames.append(extraColumnNames[EC_ExtraTemperature2] + QString(" (%1)").arg(temp));
    } if (columns.extra.testFlag(EC_ExtraTemperature3)) {
        columnNames.append(extraColumnNames[EC_ExtraTemperature3] + QString(" (%1)").arg(temp));
    } if (columns.extra.testFlag(EC_ExtraHumidity1)) {
        columnNames.append(extraColumnNames[EC_ExtraHumidity1] + "(%)");
    } if (columns.extra.testFlag(EC_ExtraHumidity2)) {
        columnNames.append(extraColumnNames[EC_ExtraHumidity2] + "(%)");
    }

    return columnNames.join(getDelimiter()) + "\n";
}


