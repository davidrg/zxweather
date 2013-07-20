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

QSet<ExportDialog::COLUMNS> ExportDialog::getColumns()
{
    QSet<COLUMNS> columns;

    if (ui->cbTimestamp->isChecked())
        columns.insert(ExportDialog::C_TIMESTAMP);
    if (ui->cbApparentTemperature->isChecked())
        columns.insert(ExportDialog::C_APPARENT_TEMPERATURE);
    if (ui->cbDewPoint->isChecked())
        columns.insert(ExportDialog::C_DEW_POINT);
    if (ui->cbHumidity->isChecked())
        columns.insert(ExportDialog::C_HUMIDITY);
    if (ui->cbIndoorHumidity->isChecked())
        columns.insert(ExportDialog::C_INDOOR_HUMIDITY);
    if (ui->cbIndoorTemperature->isChecked())
        columns.insert(ExportDialog::C_INDOOR_TEMPERATURE);
    if (ui->cbPressure->isChecked())
        columns.insert(ExportDialog::C_PRESSURE);
    if (ui->cbRainfall->isChecked())
        columns.insert(ExportDialog::C_RAINFALL);
    if (ui->cbTemperature->isChecked())
        columns.insert(ExportDialog::C_TEMPERATURE);
    if (ui->cbWindChill->isChecked())
        columns.insert(ExportDialog::C_WIND_CHILL);

    return columns;
}


// #define lazyness
#define CHECK_AND_ADD(column) if (columns.contains(column)) list.append(column)

// QSet is unordered. We want an ordered list in some places which is what
// this gives us.
QList<ExportDialog::COLUMNS> ExportDialog::columnList(QSet<COLUMNS> columns)
{
    QList<COLUMNS> list;

    CHECK_AND_ADD(C_TIMESTAMP);
    CHECK_AND_ADD(C_TEMPERATURE);
    CHECK_AND_ADD(C_APPARENT_TEMPERATURE);
    CHECK_AND_ADD(C_WIND_CHILL);
    CHECK_AND_ADD(C_DEW_POINT);
    CHECK_AND_ADD(C_HUMIDITY);
    CHECK_AND_ADD(C_INDOOR_TEMPERATURE);
    CHECK_AND_ADD(C_INDOOR_HUMIDITY);
    CHECK_AND_ADD(C_PRESSURE);
    CHECK_AND_ADD(C_RAINFALL);

    return list;
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
    if (filename.isEmpty()) reject(); // user canceled

    targetFilename = filename;

    dataSource->fetchSamples(startTime, endTime);
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
    }

    QList<COLUMNS> columns = columnList(getColumns());

    QString headerRow = getHeaderRow(columns);
    if (!headerRow.isEmpty())
        dataFile.write(headerRow.toAscii());

    QString delimiter = getDelimiter();

    for (int i = 0; i < samples.timestamp.count(); i++) {
        QStringList rowData;
        foreach (COLUMNS column, columns) {
            switch(column) {
            case C_TIMESTAMP:
                rowData.append(
                            QDateTime::fromTime_t(samples.timestampUnix.at(i))
                               .toString(Qt::ISODate));
                break;
            case C_APPARENT_TEMPERATURE:
                rowData.append(QString::number(
                                   samples.apparentTemperature.at(i),'f', 1));
                break;
            case C_DEW_POINT:
                rowData.append(QString::number(samples.dewPoint.at(i),'f', 1));
                break;
            case C_HUMIDITY:
                rowData.append(QString::number(samples.humidity.at(i)));
                break;
            case C_INDOOR_HUMIDITY:
                rowData.append(QString::number(samples.indoorHumidity.at(i)));
                break;
            case C_INDOOR_TEMPERATURE:
                rowData.append(QString::number(
                                   samples.indoorTemperature.at(i),'f', 1));
                break;
            case C_PRESSURE:
                rowData.append(QString::number(samples.pressure.at(i),'f', 1));
                break;
            case C_RAINFALL:
                rowData.append(QString::number(samples.rainfall.at(i),'f', 1));
                break;
            case C_TEMPERATURE:
                rowData.append(QString::number(
                                   samples.temperature.at(i),'f', 1));
                break;
            case C_WIND_CHILL:
                rowData.append(QString::number(
                                   samples.windChill.at(i),'f', 1));
                break;
            default:
                rowData.append("(ERROR: Unrecognised column)");
            }
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

QString ExportDialog::getHeaderRow(QList<COLUMNS> columns) {
    QStringList columnNames;

    foreach (COLUMNS column, columns) {
        switch(column) {
        case C_TIMESTAMP:
            columnNames.append("Timestamp");
            break;
        case C_APPARENT_TEMPERATURE:
            columnNames.append("Apparent Temperature");
            break;
        case C_DEW_POINT:
            columnNames.append("Dew Point");
            break;
        case C_HUMIDITY:
            columnNames.append("Humidity");
            break;
        case C_INDOOR_HUMIDITY:
            columnNames.append("Indoor Humidity");
            break;
        case C_INDOOR_TEMPERATURE:
            columnNames.append("Indoor Temperature");
            break;
        case C_PRESSURE:
            columnNames.append("Pressure");
            break;
        case C_RAINFALL:
            columnNames.append("Rainfall");
            break;
        case C_TEMPERATURE:
            columnNames.append("Temperature");
            break;
        case C_WIND_CHILL:
            columnNames.append("Wind Chill");
            break;
        default:
            columnNames.append("(Unknown)");
        }
    }

    return columnNames.join(getDelimiter()) + "\n";
}


