#include "databasedatasource.h"
#include "settings.h"
#include "database.h"

#include <QMessageBox>
#include <QSqlQuery>
#include <QSqlError>
#include <QVariant>
#include <QProgressDialog>
#include <QtDebug>
#include <QSqlRecord>

DatabaseDataSource::DatabaseDataSource(QWidget* parentWidget, QObject *parent) :
    AbstractDataSource(parentWidget, parent)
{
    signalAdapter.reset(new DBSignalAdapter(this));
    wdb_set_signal_adapter(signalAdapter.data());
    connect(signalAdapter.data(), SIGNAL(error(QString)),
            this, SLOT(dbError(QString)));

    notificationTimer.reset(new QTimer(this));
    notificationTimer->setInterval(1000);
    connect(notificationTimer.data(), SIGNAL(timeout()),
            this, SLOT(notificationPump()));
}

DatabaseDataSource::~DatabaseDataSource() {
    // Disconnect from the DB if required.
    if (notificationTimer->isActive()) {
        notificationTimer->stop();
        wdb_disconnect();
    }
}

int DatabaseDataSource::getStationId() {
    static int stationId = -1;
    static QString stationCode = "";

    QString code = Settings::getInstance().stationCode();

    if (code == stationCode) return stationId;

    stationId = -1;
    stationCode = "";

    QSqlQuery query;
    query.prepare("select station_id from station where code = :code");
    query.bindValue(":code", code);
    query.exec();

    if (!query.isActive()) {
        QMessageBox::warning(0, "Database Error", query.lastError().databaseText());
    }
    else if (query.size() == 1) {
        query.first();
        stationId = query.value(0).toInt();
        stationCode = code;
    } else {
        QMessageBox::warning(0,"Configuration Error",
                             "Invalid station code " + code);
    }

    return stationId;
}

QString DatabaseDataSource::getStationHwType() {
    static int stationId = -1;
    static QString hardwareType = "";

    int id = getStationId();

    if (id != stationId && id != -1) {
        QSqlQuery query;
        query.prepare("select t.code "
                      "from station s "
                      "inner join station_type t "
                      "  on t.station_type_id = s.station_type_id "
                      "where s.station_id = :stationId");
        query.bindValue(":stationId", stationId);
        query.exec();

        if (query.isActive() && query.size() == 1) {
            query.first();
            stationId = id;
            hardwareType = query.value(0).toString();
        }
    }
    return hardwareType;
}

QString DatabaseDataSource::buildSelectForColumns(SampleColumns columns)
{
    QString selectPart = "select time_stamp";

    if (columns.testFlag(SC_Temperature))
        selectPart += ", temperature";
    if (columns.testFlag(SC_DewPoint))
        selectPart += ", dew_point";
    if (columns.testFlag(SC_ApparentTemperature))
        selectPart += ", apparent_temperature";
    if (columns.testFlag(SC_WindChill))
        selectPart += ", wind_chill";
    if (columns.testFlag(SC_IndoorTemperature))
        selectPart += ", indoor_temperature";
    if (columns.testFlag(SC_Humidity))
        selectPart += ", relative_humidity";
    if (columns.testFlag(SC_IndoorHumidity))
        selectPart += ", indoor_relative_humidity";
    if (columns.testFlag(SC_Pressure))
        selectPart += ", absolute_pressure";
    if (columns.testFlag(SC_Rainfall))
        selectPart += ", rainfall";
    if (columns.testFlag(SC_AverageWindSpeed))
        selectPart += ", average_wind_speed";
    if (columns.testFlag(SC_GustWindSpeed))
        selectPart += ", gust_wind_speed";
    if (columns.testFlag(SC_WindDirection))
        selectPart += ", wind_direction";

    return selectPart;
}

void DatabaseDataSource::fetchSamples(SampleColumns columns,
                                      QDateTime startTime,
                                      QDateTime endTime) {
    progressDialog->setWindowTitle("Loading...");
    progressDialog->setLabelText("Initialise...");
    progressDialog->setRange(0,5);
    progressDialog->setValue(0);

    int stationId = getStationId();
    if (stationId == -1) return; // Bad station code.
    QString hwType = getStationHwType();

    progressDialog->setLabelText("Count...");
    progressDialog->setValue(1);
    if (progressDialog->wasCanceled()) return;

    QSqlQuery query;

    query.prepare("select count(*)"
                  "from sample "
                  "where station_id = :stationId "
                  "  and time_stamp >= :startTs "
                  "  and time_stamp <= :endTs");
    query.bindValue(":stationId", stationId);
    query.bindValue(":startTs", startTime);
    query.bindValue(":endTs", endTime);
    bool result = query.exec();
    if (!result || !query.isActive()) {
        QMessageBox::warning(NULL, "Database Error",
                             query.lastError().driverText());
        return;
    }
    query.first();
    int size = query.value(0).toInt();

    progressDialog->setLabelText("Query...");
    progressDialog->setValue(2);
    if (progressDialog->wasCanceled()) return;

    SampleSet samples;
    ReserveSampleSetSpace(samples, size, columns);
    samples.sampleCount = size;

    QString selectPart = buildSelectForColumns(columns);

    query.prepare(selectPart + " from sample "
                  "where station_id = :stationId "
                  "  and time_stamp >= :startTs "
                  "  and time_stamp <= :endTs"
                );
    query.bindValue(":stationId", stationId);
    query.bindValue(":startTs", startTime);
    query.bindValue(":endTs", endTime);
    query.setForwardOnly(true);
    result = query.exec();
    if (!result || !query.isActive()) {
        QMessageBox::warning(NULL, "Database Error",
                             query.lastError().driverText());
        return;
    }

    progressDialog->setLabelText("Process...");
    progressDialog->setValue(3);

    while (query.next()) {
        if (progressDialog->wasCanceled()) return;

        QSqlRecord record = query.record();

        time_t timestamp = record.value("time_stamp").toDateTime().toTime_t();
        samples.timestamp.append(timestamp);

        if (columns.testFlag(SC_Temperature))
            samples.temperature.append(record.value("temperature").toDouble());

        if (columns.testFlag(SC_DewPoint))
            samples.dewPoint.append(record.value("dew_point").toDouble());

        if (columns.testFlag(SC_ApparentTemperature))
            samples.apparentTemperature.append(
                        record.value("apparent_temperature").toDouble());

        if (columns.testFlag(SC_WindChill))
            samples.windChill.append(record.value("wind_chill").toDouble());

        if (columns.testFlag(SC_IndoorTemperature))
            samples.indoorTemperature.append(
                        record.value("indoor_temperature").toDouble());

        if (columns.testFlag(SC_Humidity))
            samples.humidity.append(
                        record.value("relative_humidity").toDouble());

        if (columns.testFlag(SC_IndoorHumidity))
            samples.indoorHumidity.append(
                        record.value("indoor_relative_humidity").toDouble());

        if (columns.testFlag(SC_Pressure))
            samples.pressure.append(
                        record.value("absolute_pressure").toDouble());

        if (columns.testFlag(SC_Rainfall))
            samples.rainfall.append(record.value("rainfall").toDouble());

        if (columns.testFlag(SC_AverageWindSpeed))
            samples.averageWindSpeed.append(
                        record.value("average_wind_speed").toDouble());

        if (columns.testFlag(SC_GustWindSpeed))
            samples.gustWindSpeed.append(
                        record.value("gust_wind_speed").toDouble());

        if (columns.testFlag(SC_WindDirection))
            // Wind direction is often null.
            if (!record.value("wind_direction").isNull())
                samples.windDirection[timestamp] =
                        record.value("wind_direction").toUInt();
    }
    progressDialog->setLabelText("Draw...");
    progressDialog->setValue(4);
    if (progressDialog->wasCanceled()) return;

    emit samplesReady(samples);
    progressDialog->setValue(5);
}


void DatabaseDataSource::connectToDB() {
    Settings& settings = Settings::getInstance();

    QString dbHostname = settings.databaseHostName();
    QString dbPort = QString::number(settings.databasePort());
    QString username = settings.databaseUsername();
    QString password = settings.databasePassword();
    QString station = settings.stationCode();

    QString target = settings.databaseName();
    if (!dbHostname.isEmpty()) {
        target += "@" + dbHostname;

        if (!dbPort.isEmpty())
            target += ":" + dbPort;
    }

    qDebug() << "Connecting to target" << target << "as user" << username;

    if (wdb_connect(target.toAscii().constData(),
                     username.toAscii().constData(),
                     password.toAscii().constData(),
                     station.toAscii().constData())) {
        notificationTimer->start();
        notificationPump(true);
    }
}

void DatabaseDataSource::dbError(QString message) {
    emit error(message);
}

void DatabaseDataSource::notificationPump(bool force) {
    if (wdb_live_data_available() || force) {
        live_data_record rec = wdb_get_live_data();

        LiveDataSet lds;
        lds.indoorTemperature = rec.indoor_temperature;
        lds.indoorHumidity = rec.indoor_relative_humidity;
        lds.temperature = rec.temperature;
        lds.humidity = rec.relative_humidity;
        lds.dewPoint = rec.dew_point;
        lds.windChill = rec.wind_chill;
        lds.apparentTemperature = rec.apparent_temperature;
        lds.pressure = rec.absolute_pressure;
        lds.windSpeed = rec.average_wind_speed;
        lds.windDirection = rec.wind_direction;
        lds.timestamp = QDateTime::fromTime_t(rec.download_timestamp);
        lds.indoorDataAvailable = true;

        if (rec.v1) {
            // The V1 schema stores wind direction as a string :(
            QString strd(rec.wind_direction_str);
            if (strd == "N") lds.windDirection = 0;
            else if (strd == "NNE") lds.windDirection = 22.5;
            else if (strd == "NE") lds.windDirection = 45;
            else if (strd == "ENE") lds.windDirection = 67.5;
            else if (strd == "E") lds.windDirection = 90;
            else if (strd == "ESE") lds.windDirection = 112.5;
            else if (strd == "SE") lds.windDirection = 135;
            else if (strd == "SSE") lds.windDirection = 157.5;
            else if (strd == "S") lds.windDirection = 180;
            else if (strd == "SSW") lds.windDirection = 202.5;
            else if (strd == "SW") lds.windDirection = 225;
            else if (strd == "WSW") lds.windDirection = 247.5;
            else if (strd == "W") lds.windDirection = 270;
            else if (strd == "WNW") lds.windDirection = 292.5;
            else if (strd == "NW") lds.windDirection = 315;
            else if (strd == "NNW") lds.windDirection = 337.5;
        } else {

            switch (rec.station_type) {
            case ST_DAVIS:
                lds.hw_type = HW_DAVIS;
                break;
            case ST_FINE_OFFSET:
                lds.hw_type = HW_FINE_OFFSET;
                break;
            case ST_GENERIC:
            default:
                lds.hw_type = HW_GENERIC;
            }

            if (lds.hw_type == HW_DAVIS) {
                lds.davisHw.barometerTrend = rec.davis_data.barometer_trend;
                lds.davisHw.consoleBatteryVoltage = rec.davis_data.console_battery;
                lds.davisHw.forecastIcon = rec.davis_data.forecast_icon;
                lds.davisHw.forecastRule = rec.davis_data.forecast_rule;
                lds.davisHw.rainRate = rec.davis_data.rain_rate;
                lds.davisHw.stormRain = rec.davis_data.storm_rain;
                lds.davisHw.stormStartDate = QDateTime::fromTime_t(rec.davis_data.current_storm_start_date).date();
                lds.davisHw.txBatteryStatus = rec.davis_data.tx_battery_status;
                lds.davisHw.stormDateValid = rec.davis_data.current_storm_start_date > 0;
            }
        }

        emit liveData(lds);
    }
}

void DatabaseDataSource::enableLiveData() {
    connectToDB();
    notificationTimer->start();
}

hardware_type_t DatabaseDataSource::getHardwareType() {
    int type = wdb_get_hardware_type();

    if (type == ST_GENERIC)
        return HW_GENERIC;
    else if (type == ST_FINE_OFFSET)
        return HW_FINE_OFFSET;
    else if (type == ST_DAVIS)
        return HW_DAVIS;

    return HW_GENERIC;
}
