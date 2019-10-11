#include "runreportdialog.h"
#include "ui_runreportdialog.h"

#include "report.h"
#include "settings.h"
#include "datasource/databasedatasource.h"
#include "datasource/webdatasource.h"
#include "datasource/dialogprogresslistener.h"
#include "reportfinisher.h"

#include <QDebug>
#include <QTreeWidgetItem>
#include <QtUiTools>
#include <QBuffer>
#include <QComboBox>
#include <QPlainTextEdit>
#include <QSpinBox>
#include <QDial>
#include <QCheckBox>
#include <QLineEdit>
#include <QGroupBox>

RunReportDialog::RunReportDialog(AbstractUrlHandler *urlHandler, QWidget *parent) :
    QDialog(parent),
    ui(new Ui::RunReportDialog)
{
    ui->setupUi(this);
    this->urlHandler = urlHandler;

    QList<Report> reports = Report::loadReports();

    qSort(reports.begin(), reports.end());

    bool isWebDs = Settings::getInstance().sampleDataSourceType() == Settings::DS_TYPE_WEB_INTERFACE;
    bool isDbDs = Settings::getInstance().sampleDataSourceType() == Settings::DS_TYPE_DATABASE;

    if (Settings::getInstance().sampleDataSourceType() == Settings::DS_TYPE_DATABASE)
        ds.reset(new DatabaseDataSource(new DialogProgressListener(this), this));
    else
        ds.reset(new WebDataSource(new DialogProgressListener(this), this));
    hardware_type_t hw_type = ds->getHardwareType();
    bool solarAvailable = ds->solarAvailable();


    QSet<QString> sensors;
    foreach (ExtraColumn c, ds->extraColumnNames().keys()) {
        switch(c) {
        case EC_LeafWetness1:
            sensors.insert("leaf_wetness");
            sensors.insert("leaf_wetness_1");
            break;
        case EC_LeafWetness2:
            sensors.insert("leaf_wetness");
            sensors.insert("leaf_wetness_2");
            break;
        case EC_LeafTemperature1:
            sensors.insert("leaf_temperature");
            sensors.insert("leaf_temperature_1");
            break;
        case EC_LeafTemperature2:
            sensors.insert("leaf_temperature");
            sensors.insert("leaf_temperature_2");
            break;
        case EC_SoilMoisture1:
            sensors.insert("soil_moisture");
            sensors.insert("soil_moisture_1");
            break;
        case EC_SoilMoisture2:
            sensors.insert("soil_moisture");
            sensors.insert("soil_moisture_2");
            break;
        case EC_SoilMoisture3:
            sensors.insert("soil_moisture");
            sensors.insert("soil_moisture_3");
            break;
        case EC_SoilMoisture4:
            sensors.insert("soil_moisture");
            sensors.insert("soil_moisture_4");
            break;
        case EC_SoilTemperature1:
            sensors.insert("soil_temperature");
            sensors.insert("soil_temperature_1");
            break;
        case EC_SoilTemperature2:
            sensors.insert("soil_temperature");
            sensors.insert("soil_temperature_2");
            break;
        case EC_SoilTemperature3:
            sensors.insert("soil_temperature");
            sensors.insert("soil_temperature_3");
            break;
        case EC_SoilTemperature4:
            sensors.insert("soil_temperature");
            sensors.insert("soil_temperature_4");
            break;
        case EC_ExtraHumidity1:
            sensors.insert("extra_humidity");
            sensors.insert("extra_humidity_1");
            break;
        case EC_ExtraHumidity2:
            sensors.insert("extra_humidity");
            sensors.insert("extra_humidity_2");
            break;
        case EC_ExtraTemperature1:
            sensors.insert("extra_temperature");
            sensors.insert("extra_temperature_1");
            break;
        case EC_ExtraTemperature2:
            sensors.insert("extra_temperature");
            sensors.insert("extra_temperature_2");
            break;
        case EC_ExtraTemperature3:
            sensors.insert("extra_temperature");
            sensors.insert("extra_temperature_3");
            break;
        case EC_NoColumns:
        default:
            break; // Nothing to do.
        }
    }

    // zxweather doesn't currently support enabling UV and Solar Radiation
    // separately at the moment. It just has a "Vantage Pro2 Plus" station
    // type which implies both UV and Solar Radiation. Eventually we'll
    // let them be configured separately like we do with the other optional
    // davis sensors.
    if (solarAvailable) {
        sensors << "uv_index"
                << "solar_radiation"
                << "evapotranspiration"
                << "high_uv_index"
                << "high_solar_radiation";
    }

    if (ds->getStationInfo().isValid && ds->getStationInfo().isWireless) {
        sensors.insert("wireless_reception");
    }

    // A few extra values (not sensors) available for Davis stations. These
    // are all highs and lows for the archive interval.
    if (hw_type == HW_DAVIS) {
        sensors << "high_temperature"
                << "low_temperature"
                << "high_rain_rate"
                << "gust_wind_direction"
                << "forecast_rule_id";
    }

    // All supported stations have these
    sensors << "temperature"
            << "indoor_temperature"
            << "humidity"
            << "indoor_humidity"
            << "pressure"
            << "rainfall"
            << "average_wind_speed"
            << "gust_wind_speed"
            << "average wind direction";

    qDebug() << "Configured sensors:" << sensors;

    foreach (Report r, reports) {
        if (r.isNull())
            continue;

        if (isWebDs && !r.supportsWebDS()) {
            continue; // Report not compatible with the current data source
        }

        if (isDbDs && !r.supportsDBDS()) {
            continue; // Report not compatible with the current data source
        }

        QSet<Report::WeatherStationType> wsType = r.supportedWeatherStations();

        // If the report needs something fancier than a generic weather station
        // check the weather station type is listed as supported by the report.
        if (!wsType.contains(Report::WST_Generic)) {
            switch(hw_type) {
            case HW_DAVIS:
                // We're either a Vantage Pro2/Vue or Vantage Pro2 Plus (or Vue
                // console with a Pro2 Plus SIM)

                if (wsType.contains(Report::WST_VantagePro2)) {
                    // Any davis station is fine.
                    break;
                } else if (solarAvailable &&  wsType.contains(Report::WST_VantagePro2Plus)) {
                    // Solar sensors are required and we have them.
                    break;
                }

                // Either davis stations aren't supported or the report needs
                // solar sensors and we don't have them.
                continue;
            case HW_FINE_OFFSET:
                if (!wsType.contains(Report::WST_WH1080)) {
                    continue; // No support for Fine offset stations.
                }
                break;
            case HW_GENERIC:
            default:
                continue; // Generic wasn't listed
            }
        }

        qDebug() << "Report requires sensors:" << r.requiredSensors();
        bool hasRequiredSensors = true;
        foreach (QString sensor, r.requiredSensors()) {
            if (!sensors.contains(sensor)) {
                qDebug() << "Required sensor" << sensor << "not available - report not compatible!";
                hasRequiredSensors = false;
                break;
            }
        }
        if (!hasRequiredSensors) {
            // Skip report
            continue;
        }

        QTreeWidgetItem *twi = new QTreeWidgetItem();
        twi->setText(0, r.title());
        twi->setData(0, Qt::UserRole, r.name());
        twi->setIcon(0, r.icon());
        ui->treeWidget->addTopLevelItem(twi);
    }

    connect(ui->treeWidget, SIGNAL(currentItemChanged(QTreeWidgetItem*,QTreeWidgetItem*)),
            this, SLOT(reportSelected(QTreeWidgetItem*,QTreeWidgetItem*)));
    connect(ui->treeWidget, SIGNAL(doubleClicked(QModelIndex)),
            this, SLOT(moveNextPage()));
    connect(ui->pbCancel, SIGNAL(clicked(bool)), this, SLOT(cancel()));
    connect(ui->pbNext, SIGNAL(clicked(bool)), this, SLOT(moveNextPage()));
    connect(ui->pbBack, SIGNAL(clicked(bool)), this, SLOT(movePreviousPage()));

    connect(ui->rbDate, SIGNAL(toggled(bool)), this, SLOT(timespanSelected()));
    connect(ui->rbDateSpan, SIGNAL(toggled(bool)), this, SLOT(timespanSelected()));
    connect(ui->rbLastMonth, SIGNAL(toggled(bool)), this, SLOT(timespanSelected()));
    connect(ui->rbLastWeek, SIGNAL(toggled(bool)), this, SLOT(timespanSelected()));
    connect(ui->rbLastYear, SIGNAL(toggled(bool)), this, SLOT(timespanSelected()));
    connect(ui->rbMonth, SIGNAL(toggled(bool)), this, SLOT(timespanSelected()));
    connect(ui->rbThisMonth, SIGNAL(toggled(bool)), this, SLOT(timespanSelected()));
    connect(ui->rbThisWeek, SIGNAL(toggled(bool)), this, SLOT(timespanSelected()));
    connect(ui->rbThisYear, SIGNAL(toggled(bool)), this, SLOT(timespanSelected()));
    connect(ui->rbTimeSpan, SIGNAL(toggled(bool)), this, SLOT(timespanSelected()));
    connect(ui->rbToday, SIGNAL(toggled(bool)), this, SLOT(timespanSelected()));
    connect(ui->rbYear, SIGNAL(toggled(bool)), this, SLOT(timespanSelected()));
    connect(ui->rbYesterday, SIGNAL(toggled(bool)), this, SLOT(timespanSelected()));
    connect(ui->rbAllTime, SIGNAL(toggled(bool)), this, SLOT(timespanSelected()));

    ui->pbNext->setEnabled(false);
    ui->pbBack->setEnabled(false);

    previousPage = Page_None;
    nextPage = Page_None;

    QDate d = QDate::currentDate();
    ui->deEndDate->setDate(d);
    ui->deStartDate->setDate(d.addDays(-7));
    ui->deDate->setDate(d.addDays(-2));
    d.setDate(d.year(), d.month(), 1);
    ui->deMonth->setDate(d.addMonths(-2));
    d.setDate(d.year(), 1, 1);
    ui->deYear->setDate(d.addYears(-2));

    QDateTime t = QDateTime::currentDateTime();
    ui->teEndTime->setDateTime(t);
    ui->teStartTime->setDateTime(t.addDays(-7));

    ui->splitter->setStretchFactor(0, 1);
    ui->splitter->setStretchFactor(1, 2);

    ui->custom_criteria_page->setLayout(new QGridLayout());
}

RunReportDialog::~RunReportDialog()
{
    qDebug() << "RunReportDialog - destructor";
    delete ui;
}

void RunReportDialog::loadReportCriteria() {
    QVariantMap params = Settings::getInstance().getReportCriteria(report.name());

    station_info_t info = ds->getStationInfo();

    if (info.coordinatesPresent) {
        params["latitude"] = info.latitude;
        params["longitude"] = info.longitude;
    }

    if (info.altitude > 0) {
        params["altitude"] = info.altitude;
    }

    if (!info.title.isNull()) {
        params["title"] = info.title;
    }

    if (!info.description.isNull()) {
        params["description"] = info.description;
    }

    time_span_t span = get_time_span();
    params["start"] = span.start;
    params["end"] = span.end;
    params["year"] = get_year();
    params["month"] = get_month();
    params["date"] = get_date();
    params["reportName"] = this->report.title();
    params["reportNameBig"] = "<h1>" + this->report.title() + "</h1>";

    qDebug() << params;

    QList<QLineEdit*> lineEdits = ui->custom_criteria_page->findChildren<QLineEdit*>();
    QList<QComboBox*> comboBoxes = ui->custom_criteria_page->findChildren<QComboBox*>();
    QList<QTextEdit*> textEdits = ui->custom_criteria_page->findChildren<QTextEdit*>();
    QList<QPlainTextEdit*> plainTextEdits = ui->custom_criteria_page->findChildren<QPlainTextEdit*>();
    QList<QSpinBox*> spinBoxes = ui->custom_criteria_page->findChildren<QSpinBox*>();
    QList<QDoubleSpinBox*> doubleSpinBoxes = ui->custom_criteria_page->findChildren<QDoubleSpinBox*>();
    QList<QTimeEdit*> timeEdits = ui->custom_criteria_page->findChildren<QTimeEdit*>();
    QList<QDateEdit*> dateEdits = ui->custom_criteria_page->findChildren<QDateEdit*>();
    QList<QDateTimeEdit*> dateTimeEdits = ui->custom_criteria_page->findChildren<QDateTimeEdit*>();
    QList<QDial*> dials = ui->custom_criteria_page->findChildren<QDial*>();
    QList<QSlider*> sliders = ui->custom_criteria_page->findChildren<QSlider*>();
    QList<QRadioButton*> radioButtons = ui->custom_criteria_page->findChildren<QRadioButton*>();
    QList<QCheckBox*> checkBoxes = ui->custom_criteria_page->findChildren<QCheckBox*>();
    QList<QGroupBox*> groupBoxes = ui->custom_criteria_page->findChildren<QGroupBox*>();
    QList<QLabel*> labels = ui->custom_criteria_page->findChildren<QLabel*>();

    // These map sensor name to sensor display name
    QMap<QString, QString> temperature_sensors;
    QMap<QString, QString> humidity_sensors;
    QMap<QString, QString> leaf_wetness_sensors;
    QMap<QString, QString> leaf_temperature_sensors;
    QMap<QString, QString> soil_moisture_sensors;
    QMap<QString, QString> soil_temperature_sensors;

    // And this is the order we sort sensors in. This is because we want
    // the built-in sensors to come before others.
    QList<QString> temperature_sensors_keys;
    QList<QString> humidity_sensors_keys;
    QList<QString> leaf_wetness_sensors_keys;
    QList<QString> leaf_temperature_sensors_keys;
    QList<QString> soil_moisture_sensors_keys;
    QList<QString> soil_temperature_sensors_keys;

    temperature_sensors["outdoor_temperature"] = tr("Outside Temperature");
    temperature_sensors["indoor_temperature"] = tr("Inside Temperature");
    temperature_sensors_keys << "outdoor_temperature" << "indoor_temperature";

    humidity_sensors["outdoor_humidity"] = tr("Outside Humidity");
    humidity_sensors["indoor_humidity"] = tr("Inside Humidity");
    humidity_sensors_keys << "outdoor_humidity" << "indoor_humidity";

    QMap<ExtraColumn, QString> extra_columns = ds->extraColumnNames();
    foreach (ExtraColumn ec, extra_columns.keys()) {
        QString sensorName = "";
        switch (ec) {
        case EC_ExtraHumidity1:
            sensorName = "extra_humidity_1";
            humidity_sensors[sensorName] = extra_columns[ec];
            humidity_sensors_keys << sensorName;
            break;
        case EC_ExtraHumidity2:
            sensorName = "extra_humidity_2";
            humidity_sensors[sensorName] = extra_columns[ec];
            humidity_sensors_keys << sensorName;
            break;
        case EC_ExtraTemperature1:
            sensorName = "extra_temperature_1";
            temperature_sensors[sensorName] = extra_columns[ec];
            temperature_sensors_keys << sensorName;
            break;
        case EC_ExtraTemperature2:
            sensorName = "extra_temperature_2";
            temperature_sensors[sensorName] = extra_columns[ec];
            temperature_sensors_keys << sensorName;
            break;
        case EC_ExtraTemperature3:
            sensorName = "extra_temperature_3";
            temperature_sensors[sensorName] = extra_columns[ec];
            temperature_sensors_keys << sensorName;
            break;
        case EC_LeafTemperature1:
            sensorName = "leaf_temperature_1";
            leaf_temperature_sensors[sensorName] = extra_columns[ec];
            leaf_temperature_sensors_keys << sensorName;
            temperature_sensors[sensorName] = extra_columns[ec];
            temperature_sensors_keys << sensorName;
            break;
        case EC_LeafTemperature2:
            sensorName = "leaf_temperature_2";
            leaf_temperature_sensors[sensorName] = extra_columns[ec];
            leaf_temperature_sensors_keys << sensorName;
            temperature_sensors[sensorName] = extra_columns[ec];
            temperature_sensors_keys << sensorName;
            break;
        case EC_LeafWetness1:
            sensorName = "leaf_wetness_1";
            leaf_wetness_sensors[sensorName] = extra_columns[ec];
            leaf_wetness_sensors_keys << sensorName;
            break;
        case EC_LeafWetness2:
            sensorName = "leaf_wetness_2";
            leaf_wetness_sensors[sensorName] = extra_columns[ec];
            leaf_wetness_sensors_keys << sensorName;
            break;
        case EC_SoilMoisture1:
            sensorName = "soil_moisture_1";
            soil_moisture_sensors[sensorName] = extra_columns[ec];
            soil_moisture_sensors_keys << sensorName;
            break;
        case EC_SoilMoisture2:
            sensorName = "soil_moisture_2";
            soil_moisture_sensors[sensorName] = extra_columns[ec];
            soil_moisture_sensors_keys << sensorName;
            break;
        case EC_SoilMoisture3:
            sensorName = "soil_moisture_3";
            soil_moisture_sensors[sensorName] = extra_columns[ec];
            soil_moisture_sensors_keys << sensorName;
            break;
        case EC_SoilMoisture4:
            sensorName = "soil_moisture_4";
            soil_moisture_sensors[sensorName] = extra_columns[ec];
            soil_moisture_sensors_keys << sensorName;
            break;
        case EC_SoilTemperature1:
            sensorName = "soil_temperature_1";
            temperature_sensors[sensorName] = extra_columns[ec];
            temperature_sensors_keys << sensorName;
            soil_temperature_sensors[sensorName] = extra_columns[ec];
            soil_temperature_sensors_keys << sensorName;
            break;
        case EC_SoilTemperature2:
            sensorName = "soil_temperature_2";
            temperature_sensors[sensorName] = extra_columns[ec];
            temperature_sensors_keys << sensorName;
            soil_temperature_sensors[sensorName] = extra_columns[ec];
            soil_temperature_sensors_keys << sensorName;
            break;
        case EC_SoilTemperature3:
            sensorName = "soil_temperature_3";
            temperature_sensors[sensorName] = extra_columns[ec];
            temperature_sensors_keys << sensorName;
            soil_temperature_sensors[sensorName] = extra_columns[ec];
            soil_temperature_sensors_keys << sensorName;
            break;
        case EC_SoilTemperature4:
            sensorName = "soil_temperature_4";
            temperature_sensors[sensorName] = extra_columns[ec];
            temperature_sensors_keys << sensorName;
            soil_temperature_sensors[sensorName] = extra_columns[ec];
            soil_temperature_sensors_keys << sensorName;
            break;
        case EC_NoColumns:
            continue;
        }
    }

    // Populate any combo boxes that want populating
    foreach (QComboBox* comboBox, comboBoxes) {
        QVariant optionsV = comboBox->property("options");

        if (!optionsV.isValid()) {
            continue; // No options requested
        }

        QMap<QString, QString> sensors;
        QStringList sensorSort;

        QString options = optionsV.toString().toLower();
        if (options == "temperature-sensors") {
            sensors = temperature_sensors;
            sensorSort = temperature_sensors_keys;
        } else if (options == "humidity-sensors") {
            sensors = humidity_sensors;
            sensorSort = humidity_sensors_keys;
        } else if (options == "leaf-wetness-sensors") {
            sensors = leaf_wetness_sensors;
            sensorSort = leaf_wetness_sensors_keys;
        } else if (options == "leaf-temperature-sensors") {
            sensors = leaf_temperature_sensors;
            sensorSort = leaf_temperature_sensors_keys;
        } else if (options == "soil-moisture-sensors") {
            sensors = soil_moisture_sensors;
            sensorSort = soil_moisture_sensors_keys;
        } else if (options == "soil-temperature-sensors") {
            sensors = soil_temperature_sensors;
            sensorSort = soil_temperature_sensors_keys;
        }

        foreach (QString sensor, sensorSort) {
            comboBox->addItem(sensors[sensor], sensor);
        }
    }

    foreach (QLabel* lbl, labels) {
        if (params.contains(lbl->objectName())) {
            lbl->setText(params[lbl->objectName()].toString());
        }
    }

    foreach (QLineEdit* ed, lineEdits) {
        if (params.contains(ed->objectName())) {
            ed->setText(params[ed->objectName()].toString());
        }
    }

    foreach (QComboBox* comboBox, comboBoxes) {
        QString objectName = comboBox->objectName();

        if (params.contains(objectName + "_value")) {
            comboBox->setCurrentIndex(
                        comboBox->findData(
                            params[objectName + "_value"].toString(),
                            Qt::UserRole));
        } else if (params.contains(objectName + "_id")) {
            comboBox->setCurrentIndex(params[objectName + "_id"].toInt());
        } else if (params.contains(objectName)) {
            #if (QT_VERSION >= QT_VERSION_CHECK(5,0,0))
            comboBox->setCurrentText(params[objectName].toString());
            #else
            comboBox->setEditText(params[objectName].toString());
            #endif
        }
    }

    foreach (QTextEdit* ed, textEdits) {
        if (params.contains(ed->objectName())) {
            ed->setHtml(params[ed->objectName()].toString());
        }
    }

    foreach (QPlainTextEdit *ed, plainTextEdits) {
        if (params.contains(ed->objectName())) {
            ed->setPlainText(params[ed->objectName()].toString());
        }
    }

    foreach (QSpinBox* sb, spinBoxes) {
        if (params.contains(sb->objectName())) {
            sb->setValue(params[sb->objectName()].toInt());
        }
    }

    foreach (QDoubleSpinBox *sb, doubleSpinBoxes) {
        if (params.contains(sb->objectName())) {
            sb->setValue(params[sb->objectName()].toDouble());
        }
    }

    foreach (QTimeEdit *ed, timeEdits) {
        if (params.contains(ed->objectName())) {
            ed->setTime(params[ed->objectName()].toTime());
        }
    }

    foreach (QDateEdit *ed, dateEdits) {
        if (params.contains(ed->objectName())) {
            ed->setDate(params[ed->objectName()].toDate());
        }
    }

    foreach (QDateTimeEdit *ed, dateTimeEdits) {
        if (params.contains(ed->objectName())) {
            ed->setDateTime(params[ed->objectName()].toDateTime());
        }
    }

    foreach (QDial* dial, dials) {
        if (params.contains(dial->objectName())) {
            dial->setValue(params[dial->objectName()].toInt());
        }
    }

    foreach (QSlider* slider, sliders) {
        if (params.contains(slider->objectName())) {
            slider->setValue(params[slider->objectName()].toInt());
        }
    }

    foreach (QRadioButton* button, radioButtons) {
        if (params.contains(button->objectName())) {
            button->setChecked(params[button->objectName()].toBool());
        }
    }

    foreach (QCheckBox* checkbox, checkBoxes) {
        if (params.contains(checkbox->objectName())) {
            checkbox->setChecked(params[checkbox->objectName()].toBool());
        }
    }

    foreach(QGroupBox* box, groupBoxes) {
        if (box->isCheckable() && params.contains(box->objectName())) {
            box->setChecked(params[box->objectName()].toBool());
        }
    }
}

void RunReportDialog::createReportCriteria() {
    // Remove any custom criteria widgets currently in the UI.
    #if (QT_VERSION >= QT_VERSION_CHECK(5,0,0))
    QList<QWidget*> widgets = ui->custom_criteria_page->findChildren<QWidget*>(QString(), Qt::FindDirectChildrenOnly);
    #else
    QList<QWidget*> widgets = ui->custom_criteria_page->findChildren<QWidget*>();
    #endif
    foreach (QWidget* w, widgets) {
        ui->custom_criteria_page->layout()->removeWidget(w);
        delete w;
    }

    // Add this reports custom criteria widget if it has one
    QUiLoader loader;
    QByteArray ui_data = report.customCriteriaUi();
    QBuffer buf(&ui_data);
    if (buf.open(QIODevice::ReadOnly)) {
        QWidget *widget = loader.load(&buf, this);
        buf.close();
        ui->custom_criteria_page->layout()->addWidget(widget);
        loadReportCriteria();
        needsCriteriaPageCreated = false;
        //report.criteriaUICreated(widget);
    }
}

void RunReportDialog::reportSelected(QTreeWidgetItem* twi, QTreeWidgetItem *prev) {
    Q_UNUSED(prev);

    ui->pbNext->setEnabled(false);

    qDebug() << "Report Selected!";

    QString reportName = twi->data(0, Qt::UserRole).toString();
    report = Report(reportName);
    ui->textBrowser->setHtml(report.description());
    ui->lblReportTitle->setText("<h1>" + report.title() + "</h1>");
    switchPage(Page_ReportSelect);
    ui->pbNext->setEnabled(true);

    needsCriteriaPageCreated = report.hasCustomCriteria();
    qDebug() << "Has custom criteria" << needsCriteriaPageCreated;

    ui->rbDate->setEnabled(true);
    ui->rbDateSpan->setEnabled(true);
    ui->rbLastMonth->setEnabled(true);
    ui->rbLastWeek->setEnabled(true);
    ui->rbLastYear->setEnabled(true);
    ui->rbMonth->setEnabled(true);
    ui->rbThisMonth->setEnabled(true);
    ui->rbThisWeek->setEnabled(true);
    ui->rbThisYear->setEnabled(true);
    ui->rbTimeSpan->setEnabled(true);
    ui->rbToday->setEnabled(true);
    ui->rbYear->setEnabled(true);
    ui->rbYesterday->setEnabled(true);
    ui->rbAllTime->setEnabled(true);

    ui->pbNext->setText(tr("&Next >"));

    // The differet time parameters are inherited. Allowing a timespan implies
    // allowing a datespan (morining on start date to evening on end date) or a
    // single date (morning to evening) or a single month (start of month to end), etc.
    switch(report.timePickerType()) {
    case Report::TP_None:
    default:
        // Only disable the pickers if they'll be shown to the user. If a default
        // timespan is specified then we'll leave them on so they can be populated
        // but we'll skip showing the page to the user
        if (report.defaultTimeSpan() != Report::FTS_None) {
            // no years
            ui->rbThisYear->setEnabled(false);
            ui->rbLastYear->setEnabled(false);
            ui->rbYear->setEnabled(false);

            // no months
            ui->rbThisMonth->setEnabled(false);
            ui->rbLastMonth->setEnabled(false);
            ui->rbMonth->setEnabled(false);

            // No dates
            ui->rbToday->setEnabled(false);
            ui->rbYesterday->setEnabled(false);
            ui->rbDate->setEnabled(false);

            // no datespans
            ui->rbThisWeek->setEnabled(false);
            ui->rbLastWeek->setEnabled(false);
            ui->rbDateSpan->setEnabled(false);

            // no timespans
            ui->rbTimeSpan->setEnabled(false);

            // No big range
            ui->rbAllTime->setEnabled(false);
        }
        if (!report.hasCustomCriteria()) {
            ui->pbNext->setText(tr("&Finish"));
        }
        break;
    case Report::TP_Year:
        // years are ok

        // no months
        ui->rbThisMonth->setEnabled(false);
        ui->rbLastMonth->setEnabled(false);
        ui->rbMonth->setEnabled(false);

        // No dates
        ui->rbToday->setEnabled(false);
        ui->rbYesterday->setEnabled(false);
        ui->rbDate->setEnabled(false);

        // no datespans
        ui->rbThisWeek->setEnabled(false);
        ui->rbLastWeek->setEnabled(false);
        ui->rbDateSpan->setEnabled(false);

        // no timespans
        ui->rbTimeSpan->setEnabled(false);

        // No big range
        ui->rbAllTime->setEnabled(false);
        break;
    case Report::TP_Month:
        // no years
        ui->rbThisYear->setEnabled(false);
        ui->rbLastYear->setEnabled(false);
        ui->rbYear->setEnabled(false);

        // months are ok

        // No dates
        ui->rbToday->setEnabled(false);
        ui->rbYesterday->setEnabled(false);
        ui->rbDate->setEnabled(false);

        // no datespans
        ui->rbThisWeek->setEnabled(false);
        ui->rbLastWeek->setEnabled(false);
        ui->rbDateSpan->setEnabled(false);

        // no timespans
        ui->rbTimeSpan->setEnabled(false);

        // No big range
        ui->rbAllTime->setEnabled(false);
        break;
    case Report::TP_Day:
        // no years
        ui->rbThisYear->setEnabled(false);
        ui->rbLastYear->setEnabled(false);
        ui->rbYear->setEnabled(false);

        // no months
        ui->rbThisMonth->setEnabled(false);
        ui->rbLastMonth->setEnabled(false);
        ui->rbMonth->setEnabled(false);

        // days are ok

        // no datespans
        ui->rbThisWeek->setEnabled(false);
        ui->rbLastWeek->setEnabled(false);
        ui->rbDateSpan->setEnabled(false);

        // no timespans
        ui->rbTimeSpan->setEnabled(false);

        // No big range
        ui->rbAllTime->setEnabled(false);
        break;
    case Report::TP_Datespan:
        // years are ok (this is just a datespan from start to end of year)

        // months are ok (this is just a datespan from start to end of month)

        // dates are ok (this is just a datespan of day_1 to day_1)

        // date spans are ok

        // no timespans
        ui->rbTimeSpan->setEnabled(false);

        // big range is ok
        break;
    case Report::TP_Timespan:
        // Disable nothing!
        break;
    }

    switch (report.defaultTimeSpan()) {
    case Report::FTS_Today:
        ui->rbToday->setEnabled(true);
        ui->rbToday->setChecked(true);
        break;
    case Report::FTS_Yesterday:
        ui->rbYesterday->setEnabled(true);
        ui->rbYesterday->setChecked(true);
        break;
    case Report::FTS_Last_24H:
        ui->rbTimeSpan->setEnabled(true);
        ui->rbTimeSpan->setChecked(true);
        ui->teStartTime->setDateTime(QDateTime::currentDateTime().addSecs(24 * 60 * 60));
        ui->teEndTime->setDateTime(QDateTime::currentDateTime());
    case Report::FTS_ThisWeek:
        ui->rbThisWeek->setEnabled(true);
        ui->rbThisWeek->setChecked(true);
        break;
    case Report::FTS_LastWeek:
        ui->rbLastWeek->setEnabled(true);
        ui->rbLastWeek->setChecked(true);
        break;
    case Report::FTS_Last_7D:
        ui->rbTimeSpan->setEnabled(true);
        ui->rbTimeSpan->setChecked(true);
        ui->teStartTime->setDateTime(QDateTime::currentDateTime().addSecs(7 * 24 * 60 * 60));
        ui->teEndTime->setDateTime(QDateTime::currentDateTime());
    case Report::FTS_Last_14D:
        ui->rbTimeSpan->setEnabled(true);
        ui->rbTimeSpan->setChecked(true);
        ui->teStartTime->setDateTime(QDateTime::currentDateTime().addSecs(14 * 24 * 60 * 60));
        ui->teEndTime->setDateTime(QDateTime::currentDateTime());
    case Report::FTS_ThisMonth:
        ui->rbThisMonth->setEnabled(true);
        ui->rbThisMonth->setChecked(true);
        break;
    case Report::FTS_LastMonth:
        ui->rbLastMonth->setEnabled(true);
        ui->rbLastMonth->setChecked(true);
        break;
    case Report::FTS_Last_30D:
        ui->rbTimeSpan->setEnabled(true);
        ui->rbTimeSpan->setChecked(true);
        ui->teStartTime->setDateTime(QDateTime::currentDateTime().addSecs(30 * 24 * 60 * 60));
        ui->teEndTime->setDateTime(QDateTime::currentDateTime());
    case Report::FTS_ThisYear:
        ui->rbThisYear->setEnabled(true);
        ui->rbThisYear->setChecked(true);
        break;
    case Report::FTS_LastYear:
        ui->rbLastYear->setEnabled(true);
        ui->rbLastYear->setChecked(true);
        break;
    case Report::FTS_Last_365D:
        ui->rbTimeSpan->setEnabled(true);
        ui->rbTimeSpan->setChecked(true);
        ui->teStartTime->setDateTime(QDateTime::currentDateTime().addSecs(365 * 24 * 60 * 60));
        ui->teEndTime->setDateTime(QDateTime::currentDateTime());
    case Report::FTS_AllTime:
        ui->rbAllTime->setEnabled(true);
        ui->rbAllTime->setChecked(true);
        break;
    case Report::FTS_None:
    default:
        break; // no default
    }

    if (report.timePickerType() == Report::TP_None && needsCriteriaPageCreated) {
        qDebug() << "No time picker specified - creating report criteria page now";
        createReportCriteria();
    }

    ui->pbNext->setEnabled(true);
}

QDate RunReportDialog::get_date() {
    if (ui->rbToday->isChecked()) {
        return QDate::currentDate();
    } else if (ui->rbYesterday->isChecked()) {
        return QDate::currentDate().addDays(-1);
    } else if (ui->rbDate->isChecked()) {
        return ui->deDate->date();
    }
    return QDate();
}

QDate RunReportDialog::get_month() {
    QDate now = QDate::currentDate();

    QDate this_month;
    this_month.setDate(now.year(), now.month(), 1)    ;

    if (ui->rbThisMonth->isChecked()) {
        return this_month;
    } else if (ui->rbLastMonth->isChecked()) {
        return this_month.addMonths(-1);
    } else if (ui->rbMonth->isChecked()) {
        return ui->deMonth->date();
    }
    return QDate();
}

int RunReportDialog::get_year() {
    int year = QDate::currentDate().year();

    if (ui->rbThisYear->isChecked()) {
        return year;
    } else if (ui->rbLastYear->isChecked()) {
        return year - 1;
    } else if (ui->rbYear->isChecked()) {
        return ui->deYear->date().year();
    }
    return 0;
}

RunReportDialog::date_span_t RunReportDialog::get_date_span() {
    RunReportDialog::date_span_t range;

    if (ui->rbToday->isChecked()
            || ui->rbYesterday->isChecked()
            || ui->rbDate->isChecked()) {
        range.start = get_date();
        range.end = range.start;
    } else if (ui->rbThisMonth->isChecked()
               || ui->rbLastMonth->isChecked()
               || ui->rbMonth->isChecked()) {
        range.start = get_month();
        range.end = range.start.addMonths(1).addDays(-1);
    } else if (ui->rbThisYear->isChecked()
               || ui->rbLastYear->isChecked()
               || ui->rbYear->isChecked()) {
        int year = get_year();
        range.start = QDate(year, 1, 1);
        range.end = range.start.addYears(1).addDays(-1);
    } else if (ui->rbThisWeek->isChecked()
               || ui->rbLastWeek->isChecked()) {

        QDate today = QDate::currentDate();
        int subtractDays = 1 - today.dayOfWeek();
        int addDays = 7 - today.dayOfWeek();
        range.start = today.addDays(subtractDays);
        range.end = today.addDays(addDays);

        if (ui->rbLastWeek->isChecked()) {
            range.start = range.start.addDays(-7);
            range.end = range.end.addDays(-7);
        }
    } else if (ui->rbDateSpan->isChecked()) {
        range.start = ui->deStartDate->date();
        range.end = ui->deEndDate->date();
    } else if (ui->rbAllTime->isChecked()) {
        range.start = QDate(2000,1,1);
        range.end = QDate(2100,1,1);
    }

    return range;
}

RunReportDialog::time_span_t RunReportDialog::get_time_span() {
    time_span_t time_span;

    if (ui->rbAllTime->isChecked() && ui->rbTimeSpan->isEnabled()) {
        time_span.start = QDateTime(QDate(2000,1,1), QTime(0,0,0));
        time_span.end = QDateTime(QDate(2100,1,1), QTime(0,0,0));
        return time_span;
    }

    if (ui->rbToday->isChecked()
            || ui->rbYesterday->isChecked()
            || ui->rbDate->isChecked()
            || ui->rbThisMonth->isChecked()
            || ui->rbLastMonth->isChecked()
            || ui->rbMonth->isChecked()
            || ui->rbThisYear->isChecked()
            || ui->rbLastYear->isChecked()
            || ui->rbYear->isChecked()
            || ui->rbThisWeek->isChecked()
            || ui->rbLastWeek->isChecked()
            || ui->rbDateSpan->isChecked()
            || ui->rbAllTime->isChecked()) {
        date_span_t span = get_date_span();

        time_span.start = QDateTime(span.start, QTime(0, 0));
        time_span.end = QDateTime(span.end, QTime(23, 59, 59, 59));
    } else {
        time_span.start = ui->teStartTime->dateTime();
        time_span.end = ui->teEndTime->dateTime();
    }
    return time_span;
}

void RunReportDialog::moveNextPage() {
    if (nextPage == Page_Finish) {
//        if (ui->stackedWidget->currentIndex() == Page_Criteria) {
//            QString result;
//            if (!report.validateCriteriaUI(
//                        result,
//                         ui->custom_criteria_page->layout()->widget())) {
//                QMessageBox::warning(this, "Error", result);
//                return;
//            }
//        }

        qDebug() << "Running report...";
        ReportFinisher* finisher = runReport();
        if (finisher == NULL) {
            accept();
        } else {
            if (finisher->isFinished()) {
                accept();
            }
            connect(finisher, SIGNAL(reportCompleted()), this, SLOT(accept()));
            ui->pbBack->setEnabled(false);
            ui->pbNext->setEnabled(false);
            ui->pbCancel->setEnabled(false);
            ui->treeWidget->setEnabled(false);
            ui->stackedWidget->setCurrentIndex(0);
        }
    } else {
        switchPage(nextPage);
    }
}

void RunReportDialog::movePreviousPage() {
    switchPage(previousPage);
}

void RunReportDialog::cancel() {
    reject();
}

void RunReportDialog::switchPage(Page page) {
    if (page == Page_None) {
        return;
    }

    if (page == Page_Criteria && needsCriteriaPageCreated) {
        qDebug() << "switching to criteria page - creating report criteria page now";
        createReportCriteria();
    }

    // Set previous page
    ui->pbBack->setEnabled(true);
    switch(page) {
    case Page_Timespan:
        previousPage = Page_ReportSelect;
        timespanSelected();
        break;
    case Page_Criteria:
        if (report.timePickerType() == Report::TP_None) {
            previousPage = Page_ReportSelect;
        } else {
            previousPage = Page_Timespan;
        }
        break;
    case Page_None:
    case Page_ReportSelect:
    default:
        previousPage = Page_None;
        ui->pbBack->setEnabled(false);;
        break;
    }

    // Set next page
    ui->pbNext->setText(tr("&Next >"));
    switch(page) {
    case Page_Timespan:
        if (report.hasCustomCriteria()) {
            nextPage = Page_Criteria;
        } else {
            ui->pbNext->setText(tr("&Finish"));
            nextPage = Page_Finish;
        }
        break;
    case Page_Criteria:
        nextPage = Page_Finish;
        ui->pbNext->setText(tr("&Finish"));
        break;
    case Page_None:
    case Page_ReportSelect:
    default:
        if (report.timePickerType() == Report::TP_None) {
            if (report.hasCustomCriteria()) {
                nextPage = Page_Criteria;
            } else {
                nextPage = Page_Finish;
            }
        } else {
            nextPage = Page_Timespan;
        }
        break;
    }

    ui->stackedWidget->setCurrentIndex(page);
}

ReportFinisher *RunReportDialog::runReport() {
//    AbstractDataSource *ds;
//    if (Settings::getInstance().sampleDataSourceType() == Settings::DS_TYPE_DATABASE)
//        ds = new DatabaseDataSource(new DialogProgressListener(this), this);
//    else
//        ds = new WebDataSource(new DialogProgressListener(this), this);

    QVariantMap params;

    if (report.hasCustomCriteria()) {
        QList<QLineEdit*> lineEdits = ui->custom_criteria_page->findChildren<QLineEdit*>();
        QList<QComboBox*> comboBoxes = ui->custom_criteria_page->findChildren<QComboBox*>();
        QList<QTextEdit*> textEdits = ui->custom_criteria_page->findChildren<QTextEdit*>();
        QList<QPlainTextEdit*> plainTextEdits = ui->custom_criteria_page->findChildren<QPlainTextEdit*>();
        QList<QSpinBox*> spinBoxes = ui->custom_criteria_page->findChildren<QSpinBox*>();
        QList<QDoubleSpinBox*> doubleSpinBoxes = ui->custom_criteria_page->findChildren<QDoubleSpinBox*>();
        QList<QTimeEdit*> timeEdits = ui->custom_criteria_page->findChildren<QTimeEdit*>();
        QList<QDateEdit*> dateEdits = ui->custom_criteria_page->findChildren<QDateEdit*>();
        QList<QDateTimeEdit*> dateTimeEdits = ui->custom_criteria_page->findChildren<QDateTimeEdit*>();
        QList<QDial*> dials = ui->custom_criteria_page->findChildren<QDial*>();
        QList<QSlider*> sliders = ui->custom_criteria_page->findChildren<QSlider*>();
        QList<QRadioButton*> radioButtons = ui->custom_criteria_page->findChildren<QRadioButton*>();
        QList<QCheckBox*> checkBoxes = ui->custom_criteria_page->findChildren<QCheckBox*>();
        QList<QGroupBox*> groupBoxes = ui->custom_criteria_page->findChildren<QGroupBox*>();

        foreach (QLineEdit* ed, lineEdits) {
            params[ed->objectName()] = ed->text();
        }

        foreach (QComboBox* comboBox, comboBoxes) {
            params[comboBox->objectName()] = comboBox->currentText();
            params[comboBox->objectName() + "_id"] = comboBox->currentIndex();

            if (comboBox->property("options").isValid()) {
                params[comboBox->objectName() + "_value"] = comboBox->currentData(Qt::UserRole).toString();
            }
        }

        foreach (QTextEdit* ed, textEdits) {
            params[ed->objectName()] = ed->document()->toHtml();
        }

        foreach (QPlainTextEdit *ed, plainTextEdits) {
            params[ed->objectName()] = ed->document()->toPlainText();
        }

        foreach (QSpinBox* sb, spinBoxes) {
            params[sb->objectName()] = sb->value();
        }

        foreach (QDoubleSpinBox *sb, doubleSpinBoxes) {
            params[sb->objectName()] = sb->value();
        }

        foreach (QTimeEdit *ed, timeEdits) {
            params[ed->objectName()] = ed->time();
        }

        foreach (QDateEdit *ed, dateEdits) {
            params[ed->objectName()] = ed->date();
        }

        foreach (QDateTimeEdit *ed, dateTimeEdits) {
            params[ed->objectName()] = ed->dateTime();
        }

        foreach (QDial* dial, dials) {
            params[dial->objectName()] = dial->value();
        }

        foreach (QSlider* slider, sliders) {
            params[slider->objectName()] = slider->value();
        }

        foreach (QRadioButton* button, radioButtons) {
            params[button->objectName()] = button->isChecked();
        }

        foreach (QCheckBox* checkbox, checkBoxes) {
            params[checkbox->objectName()] = checkbox->isChecked();
        }

        foreach (QGroupBox* box, groupBoxes) {
            if (box->isCheckable()) {
                params[box->objectName()] = box->isChecked();
            }
        }

        Settings::getInstance().saveReportCriteria(report.name(), params);
    }

    if (report.timePickerType() == Report::TP_Timespan ||
            (report.timePickerType() == Report::TP_None &&
             report.defaultTimeSpan() != Report::FTS_None)) {
        time_span_t ts = get_time_span();
        return report.run(ds.data(), urlHandler, ts.start, ts.end, params);
    } else if (report.timePickerType() == Report::TP_Datespan) {
        date_span_t span = get_date_span();
        return report.run(ds.data(), urlHandler, span.start, span.end, params);
    } else if (report.timePickerType() == Report::TP_Day) {
        return report.run(ds.data(), urlHandler, get_date(), false, params);
    } else if (report.timePickerType() == Report::TP_Month) {
        return report.run(ds.data(), urlHandler, get_month(), true, params);
    } else if (report.timePickerType() == Report::TP_Year) {
        return report.run(ds.data(), urlHandler, get_year(), params);
    }
    qDebug() << "No time picker selected!";
    return NULL;
}

void RunReportDialog::timespanSelected() {
    bool enabled = ui->rbToday->isChecked()
            || ui->rbYesterday->isChecked()
            || ui->rbDate->isChecked()
            || ui->rbThisMonth->isChecked()
            || ui->rbLastMonth->isChecked()
            || ui->rbMonth->isChecked()
            || ui->rbThisYear->isChecked()
            || ui->rbLastYear->isChecked()
            || ui->rbYear->isChecked()
            || ui->rbThisWeek->isChecked()
            || ui->rbLastWeek->isChecked()
            || ui->rbDateSpan->isChecked()
            || ui->rbTimeSpan->isChecked()
            || ui->rbAllTime->isChecked();

    ui->pbNext->setEnabled(enabled);
}
