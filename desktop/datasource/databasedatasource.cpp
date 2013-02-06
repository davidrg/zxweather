#include "databasedatasource.h"
#include "settings.h"

#include <QMessageBox>
#include <QSqlQuery>
#include <QSqlError>
#include <QVariant>
#include <QProgressDialog>

DatabaseDataSource::DatabaseDataSource(QWidget* parentWidget, QObject *parent) :
    AbstractDataSource(parentWidget, parent)
{
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
    /*
     * Required columns:
     *  - timestamp
     *  - temperature
     *  - dewpoint
     *  - apparent temperature
     *  - wind chill
     *  - indoor temperature
     *  - humidity
     *  - indoor humidity
     *  - pressure
     *  - rainfall
     *
     * We also need to get a sample count too.
     *
     */

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
