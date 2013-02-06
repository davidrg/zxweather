#include "databasedatasource.h"
#include "settings.h"
#include "database.h"

#include <QMessageBox>
#include <QSqlQuery>
#include <QSqlError>
#include <QVariant>
#include <QProgressDialog>
#include <QtDebug>

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

void DatabaseDataSource::fetchSamples(QDateTime startTime, QDateTime endTime) {
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
    samples.sampleCount = size;
    samples.timestamp.reserve(size);
    samples.temperature.reserve(size);
    samples.dewPoint.reserve(size);
    samples.apparentTemperature.reserve(size);
    samples.windChill.reserve(size);
    samples.indoorTemperature.reserve(size);
    samples.humidity.reserve(size);
    samples.indoorHumidity.reserve(size);
    samples.pressure.reserve(size);
    samples.rainfall.reserve(size);

    query.prepare("select time_stamp, "
                  "       temperature, "
                  "       dew_point, "
                  "       apparent_temperature, "
                  "       wind_chill, "
                  "       indoor_temperature, "
                  "       relative_humidity, "
                  "        indoor_relative_humidity, "
                  "       absolute_pressure, "
                  "       rainfall "
                  "from sample "
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

        samples.timestamp.append(query.value(0).toDateTime().toTime_t());
        samples.temperature.append(query.value(1).toDouble());
        samples.dewPoint.append(query.value(2).toDouble());
        samples.apparentTemperature.append(query.value(3).toDouble());
        samples.windChill.append(query.value(4).toDouble());
        samples.indoorTemperature.append(query.value(5).toDouble());
        samples.humidity.append(query.value(6).toDouble());
        samples.indoorHumidity.append(query.value(7).toDouble());
        samples.pressure.append(query.value(8).toDouble());
        samples.rainfall.append(query.value(9).toDouble());
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
        lds.gustWindSpeed = rec.gust_wind_speed;
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
        }

        emit liveData(lds);
    }
}

void DatabaseDataSource::enableLiveData() {
    connectToDB();
    notificationTimer->start();
}
