#include "databasedatasource.h"
#include "settings.h"
#include "database.h"
#include "json/json.h"

#include <QMessageBox>
#include <QSqlQuery>
#include <QSqlError>
#include <QVariant>
#include <QtDebug>
#include <QSqlRecord>
#include <QDesktopServices>
#include <QFile>
#include <QDir>

DatabaseDataSource::DatabaseDataSource(AbstractProgressListener *progressListener, QObject *parent) :
    AbstractDataSource(progressListener, parent)
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

    QString code = Settings::getInstance().stationCode().toUpper();

    if (code == stationCode) return stationId;

    stationId = -1;
    stationCode = "";

    QSqlQuery query;
    query.prepare("select station_id from station where upper(code) = :code");
    query.bindValue(":code", code);
    query.exec();

    if (!query.isActive()) {
        databaseError("getStationId", query.lastError(), query.lastQuery());
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
        query.prepare("select upper(t.code) as code "
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

QString buildColumnList(SampleColumns columns, QString format,
                        bool qualifiers=false, QString qualifiedFormat=QString()) {
    if (qualifiers && qualifiedFormat.isNull()) {
        qualifiedFormat = format;
    }
    QString query;
    if (columns.testFlag(SC_Timestamp))
        query += format.arg("time_stamp");
    if (columns.testFlag(SC_Temperature))
        query += format.arg("temperature");
    if (columns.testFlag(SC_DewPoint))
        query += format.arg("dew_point");
    if (columns.testFlag(SC_ApparentTemperature))
        query += format.arg("apparent_temperature");
    if (columns.testFlag(SC_WindChill))
        query += format.arg("wind_chill");
    if (columns.testFlag(SC_IndoorTemperature))
        query += format.arg("indoor_temperature");
    if (columns.testFlag(SC_IndoorHumidity))
        query += format.arg("indoor_relative_humidity");
    if (columns.testFlag(SC_Humidity))
        query += format.arg("relative_humidity");
    if (columns.testFlag(SC_Pressure))
        query += format.arg("absolute_pressure");
    if (columns.testFlag(SC_AverageWindSpeed))
        query += format.arg("average_wind_speed");
    if (columns.testFlag(SC_GustWindSpeed))
        query += format.arg("gust_wind_speed");
    if (columns.testFlag(SC_WindDirection))
        query += format.arg("wind_direction");
    if (columns.testFlag(SC_Rainfall))
        query += format.arg("rainfall");
    if (columns.testFlag(SC_UV_Index)) {
        if (qualifiers) {
            query += qualifiedFormat.arg("ds.average_uv_index");
        } else {
            query += format.arg("average_uv_index");
        }
    }
    if (columns.testFlag(SC_SolarRadiation)) {
        if (qualifiers) {
            query += qualifiedFormat.arg("ds.solar_radiation");
        } else {
            query += format.arg("solar_radiation");
        }
    }
    if (columns.testFlag(SC_GustWindDirection)) {
        if (qualifiers) {
            query += qualifiedFormat.arg("ds.gust_wind_direction");
        } else {
            query += format.arg("gust_wind_direction");
        }
    }
    if (columns.testFlag(SC_Evapotranspiration)) {
        if (qualifiers) {
            query += qualifiedFormat.arg("ds.evapotranspiration");
        } else {
            query += format.arg("evapotranspiration");
        }
    }
    if (columns.testFlag(SC_HighTemperature)) {
        if (qualifiers) {
            query += qualifiedFormat.arg("ds.high_temperature");
        } else {
            query += format.arg("high_temperature");
        }
    }
    if (columns.testFlag(SC_LowTemperature)) {
        if (qualifiers) {
            query += qualifiedFormat.arg("ds.low_temperature");
        } else {
            query += format.arg("low_temperature");
        }
    }
    if (columns.testFlag(SC_HighRainRate)) {
        if (qualifiers) {
            query += qualifiedFormat.arg("ds.high_rain_rate");
        } else {
            query += format.arg("high_rain_rate");
        }
    }
    if (columns.testFlag(SC_HighSolarRadiation)) {
        if (qualifiers) {
            query += qualifiedFormat.arg("ds.high_solar_radiation");
        } else {
            query += format.arg("high_solar_radiation");
        }
    }
    if (columns.testFlag(SC_HighUVIndex)) {
        if (qualifiers) {
            query += qualifiedFormat.arg("ds.high_uv_index");
        } else {
            query += format.arg("high_uv_index");
        }
    }
    if (columns.testFlag(SC_ForecastRuleId)) {
        if (qualifiers) {
            query += qualifiedFormat.arg("ds.forecast_rule_id");
        } else {
            query += format.arg("forecast_rule_id");
        }
    }
    if (columns.testFlag(SC_Reception)) {
        if (qualifiers) {
            query += qualifiedFormat.arg(
                "case when :broadcastId is null then null "
                "else round((ds.wind_sample_count / ((st.sample_interval::float) /((41+:broadcastId-1)::float /16.0 )) * 100)::numeric,1)::float "
                "end as reception");
        } else {
            query += format.arg("reception");
        }
    }

    return query;
}

QString buildSelectForColumns(SampleColumns columns)
{
    // Unset timestamp column (we'll put that in ourselves)
    columns &= ~SC_Timestamp;

    QString query = "select time_stamp";
    query += buildColumnList(columns, ", %1", true, ", %1 ");

    return query;
}

QString buildGroupedSelect(SampleColumns columns, AggregateFunction function, AggregateGroupType groupType, uint32_t minutes) {

    QString fn = "";
    if (function == AF_Average)
        fn = "avg";
    if (function == AF_Maximum)
        fn = "max";
    if (function == AF_Minimum)
        fn = "min";
    if (function == AF_Sum || function == AF_RunningTotal)
        fn = "sum";

    // Build the outer query. This will fetch everything from the subquery 'iq'
    // and aggregate it
    QString query = "select iq.quadrant as quadrant ";

    if (columns.testFlag(SC_Timestamp))
        query += ", min(iq.time_stamp) as time_stamp ";

    // Column names in the list get wrapped in the aggregate function
    query += buildColumnList(columns & ~SC_Timestamp, QString(", %1(iq.%2) as %2 ").arg(fn).arg("%1"), false);

    // Start of subquery 'iq'
    query += " from (select ";

    // Build a quadrant number to group on
    if (groupType == AGT_Custom)
        query += "(extract(epoch from cur.time_stamp) / :groupSeconds)::integer AS quadrant ";
    else if (groupType == AGT_Month)
        query += "extract(epoch from date_trunc('month', cur.time_stamp))::integer as quadrant ";
    else // year
        query += "extract(epoch from date_trunc('year', cur.time_stamp))::integer as quadrant ";

    // Columns that we will return to the parent query. Normal columns come
    // from the current sample where as special columns (such as those unique
    // to davis hardware or columns whose value is computed) need to be included
    // in their self-qualified form in this column list. Davis-specific columns
    // will expect to come from relation 'ds'.
    query += buildColumnList(columns, ", cur.%1 ", true, ", %1 ");

    // Rest of subquery 'iq'
    query += " from sample cur "
             " join sample prev on prev.station_id = cur.station_id "
             "                 and prev.time_stamp = (select max(time_stamp) from sample where time_stamp < cur.time_stamp"
                                             "        and station_id = :stationId )"
             " inner join station st on st.station_id = cur.station_id "
             " left outer join davis_sample ds on ds.sample_id = cur.sample_id "
             " where cur.time_stamp <= :endTime"
             " and cur.time_stamp >= :startTime"
             " and cur.station_id = :stationIdB "
             " order by cur.time_stamp asc) as iq "
             " group by iq.quadrant "
             " order by iq.quadrant asc ";

    // If we're computing a running total wrap up the above query to do the job.
    if (function == AF_RunningTotal && columns.testFlag(SC_Timestamp)) {
        // This requires a window function to do it efficiently.
        // And that needs to be outside the grouped query.
        QString outer_query = "select grouped.quadrant, grouped.time_stamp ";

        outer_query += buildColumnList(columns & ~SC_Timestamp,
                                       ", sum(grouped.%1) over (order by grouped.time_stamp) as %1 ");
        outer_query += " from (" + query + ") as grouped order by grouped.time_stamp asc";
        query = outer_query;
        qDebug() << query;
    }
    /*
      resulting query parameters are:
      :stationId
      :startTime
      :endTime
      :groupSeconds (only if AGT_Custom)
     */

    qDebug() << query;

    return query;

}

QString buildGroupedCount(AggregateFunction function, AggregateGroupType groupType, uint32_t minutes) {

    QString baseQuery = buildGroupedSelect(SC_NoColumns, function, groupType, minutes);

    QString query = "select count(*) as cnt from ( " + baseQuery + " ) as x ";
    return query;
}

int DatabaseDataSource::basicCountQuery(int stationId, QDateTime startTime, QDateTime endTime) {
    QSqlQuery query;

    // TODO: this is not compatible with the v1 schema (station_id column)
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
        databaseError("basicCountQuery", query.lastError(), query.lastQuery());
        return -1;
    }
    query.first();
    return query.value(0).toInt();
}

int DatabaseDataSource::groupedCountQuery(int stationId, QDateTime startTime,
                                          QDateTime endTime,
                                          AggregateFunction function,
                                          AggregateGroupType groupType,
                                          uint32_t minutes) {
    QSqlQuery query;

    QString qry = buildGroupedCount(function, groupType, minutes);
    qDebug() << "Grouped count";
    qDebug() << "Query:" << qry;
    qDebug() << "Parameters: stationId -" << stationId << ", startTime -" << startTime
             << ", endTime -" << endTime << ", groupSeconds -" << minutes * 60;
    qDebug() << "GroupType:" << groupType << "(Custom:" << AGT_Custom << ")";

    // TODO: this is not compatible with the v1 schema (station_id column)
    query.prepare(qry);
    query.bindValue(":stationId", stationId);
    query.bindValue(":stationIdB", stationId);
    query.bindValue(":startTime", startTime);
    query.bindValue(":endTime", endTime);

    if (groupType == AGT_Custom)
        query.bindValue(":groupSeconds", minutes * 60);

    bool result = query.exec();
    if (!result || !query.isActive()) {
        qWarning() << "DB ERROR";
        databaseError("groupedCountQuery", query.lastError(), query.lastQuery());
        return -1;
    }
    result = query.first();

    int count = query.record().value("cnt").toInt();
    qDebug() << query.record();
    qDebug() << query.executedQuery();
    qDebug() << "Count:" << count;
    return count;
}

QSqlQuery setupBasicQuery(SampleColumns columns, int stationId,
                          QDateTime startTime, QDateTime endTime,
                          int broadcastId) {

    qDebug() << "Basic Query";

    QString selectPart = buildSelectForColumns(columns);    
    selectPart += " from sample "
            " inner join davis_sample ds on ds.sample_id = sample.sample_id "
            " inner join station st on st.station_id = sample.station_id "
            "where st.station_id = :stationId "
            "  and time_stamp >= :startTime "
            "  and time_stamp <= :endTime";

    // This can't be a regular query parameter as it appears in the select
    selectPart = selectPart.replace(":broadcastId", QString::number(broadcastId));

    qDebug() << "Query:" << selectPart;

    QSqlQuery query;
    query.prepare(selectPart);
    return query;
}


QSqlQuery setupGroupedQuery(SampleColumns columns, int stationId,
                            AggregateFunction function,
                            AggregateGroupType groupType,
                            uint32_t minutes, int broadcastId) {

    qDebug() << "Grouped Query";

    QString qry = buildGroupedSelect(columns, function, groupType, minutes);

    // This can't be a regular query parameter as it appears in the select
    qry = qry.replace(":broadcastId", QString::number(broadcastId));

    qDebug() << "Query:" << qry;
    qDebug() << "Parameters: stationId -" << stationId << ", groupSeconds -" << minutes * 60;
    qDebug() << "GroupType:" << groupType << "(Custom:" << AGT_Custom << ")";

    QSqlQuery query;

    query.prepare(qry);

    query.bindValue(":stationIdB", stationId);

    if (groupType == AGT_Custom)
        query.bindValue(":groupSeconds", minutes * 60);

    return query;
}

void DatabaseDataSource::fetchSamples(SampleColumns columns,
                                      QDateTime startTime,
                                      QDateTime endTime,
                                      AggregateFunction aggregateFunction,
                                      AggregateGroupType groupType,
                                      uint32_t groupMinutes) {
    progressListener->setTaskName("Loading...");
    progressListener->setSubtaskName("Initialise...");
    progressListener->setRange(0,5);
    progressListener->setValue(0);

    int stationId = getStationId();
    if (stationId == -1) return; // Bad station code.

    if (getHardwareType() != HW_DAVIS) {
        // Turn off all the davis columns - they're not valid here
        qDebug() << "Not davis hardwrae - disabling columns";
        columns = columns & ~DAVIS_COLUMNS;
    }
    // ##TODO: RECEPTION
    // TODO: also filter out reception if not wireless and solar if not Pro2Plus

    progressListener->setSubtaskName("Count...");
    progressListener->setValue(1);
    if (progressListener->wasCanceled()) return;

    int size;
    if (aggregateFunction == AF_None || groupType == AGT_None)
        size = basicCountQuery(stationId, startTime, endTime);
    else
        size = groupedCountQuery(stationId, startTime, endTime,
                                 aggregateFunction, groupType, groupMinutes);
    if (size == -1) return; // error

    progressListener->setSubtaskName("Query...");
    progressListener->setValue(2);
    if (progressListener->wasCanceled()) return;

    SampleSet samples;
    ReserveSampleSetSpace(samples, size, columns);
    samples.sampleCount = size;

    int broadcastId = -1;
    if (columns.testFlag(SC_Reception)) {
        // We need some extra config data for the reception column.

        QSqlQuery q;
        q.prepare("select station_config from station where station_id = :sid");
        q.bindValue(":sid", stationId);
        bool result = q.exec();
        if (!result) {
            // Couldn't get config data. Turn off the column
            qWarning() << "Failed to get station config for SC_Reception column. Errors:"
                       << q.lastError().driverText() << q.lastError().databaseText();
            columns = columns & ~SC_Reception;
        } else {
            q.first();
            using namespace QtJson;
            QString config = q.value(0).toString();

            bool ok;
            QVariantMap result = Json::parse(config, ok).toMap();

            if (!ok) {
                qWarning() << "Station config JSON parsing failed. Turning off reception column.";
                columns = columns & ~SC_Reception;
            } else if (!result["broadcast_id"].isNull()){
                broadcastId = result["broadcast_id"].toInt();
            }

        }

        if (broadcastId == -1) {
            qDebug() << "Failed to get broadcast id. Turning off reception column.";
            columns = columns & ~SC_Reception;
        }
    }

    QSqlQuery query;
    if (aggregateFunction == AF_None || groupType == AGT_None)
        query = setupBasicQuery(columns, stationId, startTime, endTime, broadcastId);
    else
        query = setupGroupedQuery(columns | SC_Timestamp, stationId,
                          aggregateFunction, groupType, groupMinutes,
                                  broadcastId);

    qDebug() <<  "Parameters: startTime -" << startTime << ", endTime -" << endTime;

    query.bindValue(":stationId", stationId);
    query.bindValue(":startTime", startTime);
    query.bindValue(":endTime", endTime);

//    if (columns.testFlag(SC_Reception)) {
//        query.bindValue(":broadcastIdA", broadcastId);
//        query.bindValue(":broadcastIdB", broadcastId);
//    }

    qDebug() << "Running fetch samples query";
    qDebug() << query.boundValues();

    query.setForwardOnly(true);
    bool result = query.exec();
    if (!result || !query.isActive()) {
        databaseError("fetchSamples", query.lastError(), query.lastQuery());
        return;
    }

    progressListener->setSubtaskName("Process...");
    progressListener->setValue(3);

    qDebug() << "Processing results...";
    while (query.next()) {
        if (progressListener->wasCanceled()) return;

        QSqlRecord record = query.record();

        time_t timestamp = record.value("time_stamp").toDateTime().toTime_t();
        samples.timestamp.append(timestamp);
        samples.timestampUnix.append(timestamp); // Not sure why we need both.

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

        if (columns.testFlag(SC_GustWindDirection))
            // Gust wind direction is often null.
            if (!record.value("gust_wind_direction").isNull())
                samples.gustWindDirection[timestamp] =
                        record.value("gust_wind_direction").toUInt();

        if (columns.testFlag(SC_UV_Index))
            samples.uvIndex.append(record.value("average_uv_index").toDouble());

        if (columns.testFlag(SC_SolarRadiation))
            samples.solarRadiation.append(record.value("solar_radiation").toDouble());

        if (columns.testFlag(SC_Evapotranspiration))
            samples.evapotranspiration.append(record.value("evapotranspiration").toDouble());

        if (columns.testFlag(SC_HighTemperature))
            samples.highTemperature.append(record.value("high_temperature").toDouble());

        if (columns.testFlag(SC_LowTemperature))
            samples.lowTemperature.append(record.value("low_temperature").toDouble());

        if (columns.testFlag(SC_HighRainRate))
            samples.highRainRate.append(record.value("high_rain_rate").toDouble());

        if (columns.testFlag(SC_HighSolarRadiation))
            samples.highSolarRadiation.append(record.value("high_solar_radiation").toDouble());

        if (columns.testFlag(SC_HighUVIndex))
            samples.highUVIndex.append(record.value("high_uv_index").toDouble());

        if (columns.testFlag(SC_Reception))
            samples.reception.append(record.value("reception").toDouble());

        if (columns.testFlag(SC_ForecastRuleId))
            samples.forecastRuleId.append(record.value("forecast_rule_id").toInt());
    }
    progressListener->setSubtaskName("Draw...");
    progressListener->setValue(4);
    if (progressListener->wasCanceled()) return;

    qDebug() << "Data retrieval complete.";

    emit samplesReady(samples);
    progressListener->setValue(5);
}


void DatabaseDataSource::connectToDB() {
    Settings& settings = Settings::getInstance();

    QString dbHostname = settings.databaseHostName();
    QString dbPort = QString::number(settings.databasePort());
    QString username = settings.databaseUsername();
    QString password = settings.databasePassword();
    QString station = settings.stationCode().toUpper();

    QString target = settings.databaseName();
    if (!dbHostname.isEmpty()) {
        target += "@" + dbHostname;

        if (!dbPort.isEmpty())
            target += ":" + dbPort;
    }

    qDebug() << "Connecting to target" << target << "as user" << username;

    if (wdb_connect(target.toLatin1().constData(),
                     username.toLatin1().constData(),
                     password.toLatin1().constData(),
                     station.toLatin1().constData())) {
        notificationTimer->start();
        notificationPump(true);
    }
}

void DatabaseDataSource::dbError(QString message) {
    emit error(message);
}

void DatabaseDataSource::notificationPump(bool force) {

    notifications n = wdb_live_data_available();

    if (n.live_data || force) {
        processLiveData();
    }

    if (n.new_image) {
        processNewImage(n.image_id);
    }

    if (n.new_sample) {
        processNewSample(n.sample_id);
    }
}

void DatabaseDataSource::processLiveData() {
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
            lds.davisHw.uvIndex = rec.davis_data.uv_index;
            lds.davisHw.solarRadiation = rec.davis_data.solar_radiation;
        }
    }

    emit liveData(lds);
}

void DatabaseDataSource::processNewImage(int imageId) {
    // Firstly, is it an image we care about?
    if (imageId < 0) {
        qWarning() << "Invalid image id" << imageId;
        return; // Error
    }

    qDebug() << "Fetching new image...";
    QSqlQuery query;
    query.prepare("select s.code, imgs.code, i.time_stamp \
                  from image i \
                  inner join image_source imgs on imgs.image_source_id = i.image_source_id \
                  inner join station s on s.station_id = imgs.station_id \
                  where i.image_id = :imageId ");
    query.bindValue(":imageId", imageId);
    query.exec();


    if (query.isActive() && query.size() == 1) {
        query.first();
        NewImageInfo info;
        info.stationCode = query.value(0).toString().toUpper();
        info.imageSourceCode = query.value(1).toString().toUpper();
        info.timestamp = query.value(2).toDateTime();
        info.imageId = imageId;

        if (info.stationCode != Settings::getInstance().stationCode().toUpper()) {
            qDebug() << "Image is for uninteresting station";
            return; // Image is for some other station we don't care about
        }

        qDebug() << "Got image.";
        emit newImage(info);
    }
}

void DatabaseDataSource::processNewSample(int sampleId) {
    if (sampleId < 0) {
        qWarning() << "Invalid sample id" << sampleId;
        return; // Error
    }

    // These are only used for rainfall calculations at the moment so we don't
    // bother including all the extra davis columns (plus the TCP data source
    // can't supply those either so...)

    qDebug() << "Fetching new sample...";
    QSqlQuery query;
    query.prepare("\
select s.time_stamp, s.indoor_relative_humidity, s.indoor_temperature, \
       s.relative_humidity, s.temperature, s.dew_point, s.wind_chill, \
       s.apparent_temperature, s.absolute_pressure, s.average_wind_speed, \
       s.gust_wind_speed, s.wind_direction, s.rainfall, ds.average_uv_index, \
       ds.solar_radiation \
from sample s \
left outer join davis_sample ds on ds.sample_id = s.sample_id \
where s.sample_id = :sampleId");
    query.bindValue(":sampleId", sampleId);
    query.exec();

    if (query.isActive() && query.size() == 1) {
        query.first();
        Sample s;
        s.timestamp = query.value(0).toDateTime();
        s.indoorHumidity = query.value(1).toDouble();
        s.indoorTemperature = query.value(2).toDouble();
        s.humidity = query.value(3).toDouble();
        s.temperature = query.value(4).toDouble();
        s.dewPoint = query.value(5).toDouble();
        s.windChill = query.value(6).toDouble();
        s.apparentTemperature = query.value(7).toDouble();
        s.pressure = query.value(8).toDouble();
        s.averageWindSpeed = query.value(9).toDouble();
        s.gustWindSpeed = query.value(10).toDouble();
        s.windDirectionValid = !query.value(11).isNull();
        if (s.windDirectionValid)
            s.windDirection = query.value(11).toUInt();
        s.rainfall = query.value(12).toDouble();
        s.solarRadiationValid = !query.value(13).isNull();
        if (s.solarRadiationValid)
            s.solarRadiation = query.value(13).toDouble();
        s.uvIndexValid = !query.value(14).isNull();
        if (s.uvIndexValid)
            s.uvIndex = query.value(14).toDouble();

        emit newSample(s);
    }
}

void DatabaseDataSource::fetchRainTotals() {
    QSqlQuery query;
    query.prepare(
"select day_total.day as date, day_total.total as day, \
        month_total.total as month, year_total.total as year \
from ( \
    select station_id, \
           sum(rainfall) as total, \
           date_trunc('day', time_stamp)::date as day \
     from sample \
    group by station_id, date_trunc('day', time_stamp)) as day_total \
inner join ( \
  select station_id, \
         sum(rainfall) as total, \
         date_trunc('month', time_stamp)::date as month \
   from sample \
  group by station_id, date_trunc('month', time_stamp)) as month_total \
            on month_total.station_id = day_total.station_id \
           and month_total.month = date_trunc('month', day_total.day) \
inner join ( \
  select station_id, \
         sum(rainfall) as total, \
         date_trunc('year', time_stamp)::date as year \
   from sample \
  group by station_id, date_trunc('year', time_stamp)) as year_total \
            on year_total.station_id = day_total.station_id \
           and year_total.year = date_trunc('year', day_total.day) \
where day_total.station_id = :stationId \
  and day_total.day = :date");
    query.bindValue(":stationId", getStationId());
    query.bindValue(":date", QDate::currentDate());
    query.exec();

    if (query.isActive() && query.size() == 1) {
        query.first();
        QDate date = query.value(0).toDate();
        double day = query.value(1).toDouble();
        double month = query.value(2).toDouble();
        double year = query.value(3).toDouble();
        emit rainTotalsReady(date, day, month, year);
    }
}

void DatabaseDataSource::enableLiveData() {
    using namespace QtJson;

    connectToDB();

    int id = getStationId();

    // A station ID of -1 means we're running on a v0.1 database.
    if (id != -1) {
        QSqlQuery query;
        query.prepare("select s.title, s.station_config "
                      "from station s "
                      "where s.station_id = :stationId");
        query.bindValue(":stationId", id);
        query.exec();

        if (query.isActive() && query.size() == 1) {
            query.first();
            QString title = query.value(0).toString();
            bool has_solar = false;

            QString config = query.value(1).toString();

            bool ok;
            QVariantMap result = Json::parse(config, ok).toMap();

            if (!ok) {
                emit error("JSON parsing failed");
                return;
            }

            if (result["has_solar_and_uv"].toBool()) {
                has_solar = true;
            }

            emit stationName(title);
            emit isSolarDataEnabled(has_solar);
        }
    }

    notificationTimer->start();
}

hardware_type_t DatabaseDataSource::getHardwareType() {

    QString qry = "select st.code from station_type st "
                  "inner join station s on s.station_type_id = st.station_type_id "
                  "where s.station_id = :stationId";

    QSqlQuery query;
    query.prepare(qry);
    query.bindValue(":stationId", getStationId());
    query.setForwardOnly(true);
    bool result = query.exec();
    if (!result || !query.isActive()) {
        databaseError("getHardwareType", query.lastError(), query.lastQuery());
        return HW_GENERIC;
    }
    query.first();

    QString typ = query.value(0).toString();

    if (typ.toUpper() == "DAVIS") {
        return HW_DAVIS;
    } else if (typ.toUpper() == "FOWH1080") {
        return HW_FINE_OFFSET;
    }
    return HW_GENERIC;


//    int type = wdb_get_hardware_type();

//    if (type == ST_GENERIC)
//        return HW_GENERIC;
//    else if (type == ST_FINE_OFFSET)
//        return HW_FINE_OFFSET;
//    else if (type == ST_DAVIS)
//        return HW_DAVIS;

    //return HW_GENERIC;
}

QList<ImageDate> DatabaseDataSource::getImageDates(int stationId,
                                                   int progressOffset) {
    /*
     * Our job here is to fetch a list of dates for which there is one or more
     * images.
     */

    progressListener->setSubtaskName("Query...");
    progressListener->setValue(progressOffset + 1);
    if (progressListener->wasCanceled()) return QList<ImageDate>();

    QString qry = "select inr.date_stamp as date_stamp, \n\
            -- string_agg(inr.mime_type, '|') as mime_types, \n\
            string_agg(upper(inr.src_code), '|') as image_source_codes \n\
     from ( \n\
         select distinct \n\
                img.time_stamp::date as date_stamp, \n\
                -- img.mime_type, \n\
                upper(img_src.code) as src_code, \n\
                img_src.source_name as src_name \n\
         from image img \n\
         inner join image_source img_src on img_src.image_source_id = img.image_source_id \n\
         where img_src.station_id = :stationId) as inr \n\
     group by inr.date_stamp";

    QSqlQuery query;
    query.prepare(qry);
    query.bindValue(":stationId", stationId);
    query.setForwardOnly(true);
    bool result = query.exec();
    if (!result || !query.isActive()) {
        databaseError("getImageDates", query.lastError(), query.lastQuery());
        return QList<ImageDate>();
    }

    progressListener->setSubtaskName("Process...");
    progressListener->setValue(progressOffset + 2);

    qDebug() << "Processing results...";
    QList<ImageDate> results;
    while (query.next()) {
        if (progressListener->wasCanceled()) return QList<ImageDate>();

        QSqlRecord record = query.record();

        ImageDate result;

        result.date = record.value("date_stamp").toDate();
        //result.mimeTypes = record.value("mime_types").toString().split('|');
        result.sourceCodes = record.value("image_source_codes")
                .toString().split('|');
        results << result;
    }

    return results;
}

QList<ImageSource> DatabaseDataSource::getImageSources(int stationId,
                                                       int progressOffset) {
    /*
     * Our job here is to fetch a list of dates for which there is one or more
     * images.
     */

    progressListener->setSubtaskName("Query...");
    progressListener->setValue(progressOffset + 1);
    if (progressListener->wasCanceled()) return QList<ImageSource>();

    QString qry = "select upper(code) as code, source_name, description from image_source "
                    "where station_id = :stationId";

    QSqlQuery query;
    query.prepare(qry);
    query.bindValue(":stationId", stationId);
    query.setForwardOnly(true);
    bool result = query.exec();
    if (!result || !query.isActive()) {
        databaseError("getImageSources", query.lastError(), query.lastQuery());
        return QList<ImageSource>();
    }

    progressListener->setSubtaskName("Process...");
    progressListener->setValue(progressOffset + 2);

    qDebug() << "Processing results...";
    QList<ImageSource> results;
    while (query.next()) {
        if (progressListener->wasCanceled()) return QList<ImageSource>();

        QSqlRecord record = query.record();

        ImageSource result;

        result.code = record.value("code").toString();
        result.name = record.value("source_name").toString();
        result.description = record.value("description").toString();
        results << result;
    }

    return results;
}

void DatabaseDataSource::fetchImageDateList() {

    progressListener->setTaskName("Loading...");
    progressListener->setSubtaskName("Initialise...");

    // 1 step in this function
    // 2 steps in getImageDates
    // 2 steps in getImageSources
    progressListener->setRange(0,5);
    progressListener->setValue(0);

    int stationId = getStationId();
    if (stationId == -1) return; // Bad station code.

    QList<ImageDate> imageDates = getImageDates(stationId,
                                                progressListener->value());
    if (imageDates.isEmpty()) return;

    QList<ImageSource> imageSources = getImageSources(stationId,
                                                      progressListener->value());
    if (imageSources.isEmpty()) return;
    qDebug() << "Data retrieval complete.";

    emit imageDatesReady(imageDates, imageSources);
    progressListener->close();
}

void DatabaseDataSource::fetchImageList(QDate date, QString imageSourceCode) {
    qDebug() << "Fetching list of images for" << imageSourceCode << "on" << date;
    progressListener->reset();
    progressListener->setTaskName("Loading...");
    progressListener->setSubtaskName("Initialise...");
    progressListener->setRange(0, 5);
    progressListener->setValue(0);

    // Get the
    QString qry = "select i.image_id as id, \n\
            upper(it.code) as image_type_code, \n\
            i.time_stamp, \n\
            i.title, \n\
            i.description, \n\
            i.mime_type \n\
            i.metadata \n\
     from image i \n\
     inner join image_type it on it.image_type_id = i.image_type_id \n\
     inner join image_source img_src on img_src.image_source_id = i.image_source_id \n\
     where i.time_stamp::date = :date \n\
       and upper(img_src.code) = upper(:imageSourceCode) \
     order by i.time_stamp";

    progressListener->setSubtaskName("Query...");
    progressListener->setValue(1);
    QSqlQuery query;
    query.prepare(qry);
    query.bindValue(":date", date);
    query.bindValue(":imageSourceCode", imageSourceCode);
    query.setForwardOnly(true);
    bool result = query.exec();
    if (!result || !query.isActive()) {
        databaseError("fetchImageList", query.lastError(), query.lastQuery());
        return;
    }

    progressListener->setSubtaskName("Process...");
    progressListener->setValue(2);

    qDebug() << "Processing results...";
    QList<ImageInfo> results;
    while (query.next()) {
        if (progressListener->wasCanceled()) {
            qDebug() << "Canceled";
            return;
        }

        QSqlRecord record = query.record();

        ImageInfo result;

        result.id = record.value("id").toInt();
        result.timeStamp = record.value("time_stamp").toDateTime();
        result.imageTypeCode = record.value("image_type_code").toString();
        result.title = record.value("title").toString();
        result.description = record.value("descrption").toString();
        result.mimeType = record.value("mime_type").toString();
        result.hasMetadata = !record.value("metadata").isNull();
        if (result.hasMetadata) {
            result.metadata = record.value("metadata").toByteArray();
        }

        results << result;
    }
    qDebug() << "Loaded" << results.count() << "results.";

    emit imageListReady(results);
    progressListener->close();
}

QString cacheFilename(ImageInfo imageInfo, QString stationCode) {
#if QT_VERSION >= 0x050000
    QString filename = QStandardPaths::writableLocation(
                QStandardPaths::CacheLocation);
#else
    QString filename = QDesktopServices::storageLocation(
                QDesktopServices::CacheLocation);
#endif

    filename += "/images/" +
            stationCode.toLower() + "/" +
            imageInfo.imageSource.code.toLower() + "/" +
            imageInfo.imageTypeCode.toLower() + "/" +
            QString::number(imageInfo.timeStamp.date().year()) + "/" +
            QString::number(imageInfo.timeStamp.date().month()) + "/";

    // Make sure the target directory actually exists.
    if (!QDir(filename).exists())
        QDir().mkpath(filename);

    filename += QString::number(imageInfo.timeStamp.date().day()) + "_" +
            QString::number(imageInfo.timeStamp.time().hour()) + "_" +
            QString::number(imageInfo.timeStamp.time().minute()) + "_" +
            QString::number(imageInfo.timeStamp.time().second()) + "_full.";

    // Extension doesn't really matter too much
    if (imageInfo.mimeType == "image/jpeg")
        filename += "jpeg";
    else if (imageInfo.mimeType == "image/png")
        filename += "png";
    else if (imageInfo.mimeType == "video/mp4")
        filename += "mp4";
    else if (imageInfo.mimeType == "audio/wav")
        filename += "wav";
    else if (imageInfo.mimeType == "audio/mpeg")
        filename += "mp3";
    else if (imageInfo.mimeType == "audio/flac")
        filename += "flac";
    else if (imageInfo.mimeType == "audio/ogg")
        filename += "oga";
    else
        filename += "dat";

    return filename;
}

void DatabaseDataSource::fetchImages(QList<int> imageIds, bool thumbnail) {
    QStringList idList;
    foreach (int id, imageIds) {
        idList << QString::number(id);
    }
    QString idArray = "{" + idList.join(",") + "}";

    QString qry = "select i.image_id, i.image_data, i.time_stamp, \
                          i.title, i.description, i.mime_type, \
                          upper(imgs.code) as src_code, imgs.source_name, \
                          upper(it.code) as image_type_code, i.metadata, \
                          it.type_name as image_type_name \
                   from image i \
                   inner join image_source imgs on imgs.image_source_id = i.image_source_id \
                   inner join image_type it on it.image_type_id = i.image_type_id \
                   where i.image_id = any(:idArray) order by time_stamp";

    QSqlQuery query;
    query.prepare(qry);
    query.bindValue(":idArray", idArray);
    query.setForwardOnly(true);
    bool result = query.exec();
    if (!result || !query.isActive()) {
        databaseError("fetchImages", query.lastError(), query.lastQuery());
        return;
    }

    // TODO: Not this
    QString stationCode = Settings::getInstance().stationCode().toUpper();

    qDebug() << "Processing results...";
    while (query.next()) {
        QSqlRecord record = query.record();

        int imageId = record.value("image_id").toInt();

        ImageInfo info;
        info.id = imageId;
        info.timeStamp = record.value("time_stamp").toDateTime();
        info.title = record.value("title").toString();
        info.description = record.value("description").toString();
        info.mimeType = record.value("mime_type").toString();
        info.imageSource.code = record.value("src_code").toString();
        info.imageSource.name = record.value("source_name").toString();
        info.imageTypeCode = record.value("image_type_code").toString();
        info.hasMetadata = !record.value("metadata").isNull();
        info.imageTypeName = record.value("image_type_name").toString();

        if (info.hasMetadata) {
            info.metadata = record.value("metadata").toByteArray();
        }
        QByteArray imageData = record.value("image_data").toByteArray();

        QImage srcImage;
        if (!info.mimeType.startsWith("video/")) {
            srcImage = QImage::fromData(imageData);
        }

        /*
         * Videos are passed to clients on disk rather than in memory. So we'll
         * cache the video in the usual cache location and pass out that file
         * name. The video will persist on disk until the caches are cleared by
         * the same mechanism used to manage the web data source cache.
         */
        QString filename;
        if (info.mimeType.startsWith("video/")) {
            filename = cacheFilename(info, stationCode);

            QFile file(filename);

            if (!file.exists()) {
                if (file.open(QIODevice::WriteOnly)) {
                    file.write(imageData);
                    file.close();
                }
            }
        }

        if (thumbnail) {
            // Don't try to thumbnail videos
            if (!info.mimeType.startsWith("video/")) {
                qDebug() << "Thumbnailing image" << imageId;
                // Resize the image
                QImage thumbnailImage = srcImage.scaled(THUMBNAIL_WIDTH,
                                                        THUMBNAIL_HEIGHT,
                                                        Qt::KeepAspectRatio);

                emit thumbnailReady(imageId, thumbnailImage);
            }
            emit imageReady(info, srcImage, filename);
        } else {
            emit imageReady(info, srcImage, filename);
        }
    }
}

void DatabaseDataSource::fetchImage(int imageId) {
    QList<int> imageIds;
    imageIds << imageId;
    fetchImages(imageIds, false);
}

void DatabaseDataSource::fetchThumbnails(QList<int> imageIds) {
    qDebug() << "Fetching thumbnails for" << imageIds;
    fetchImages(imageIds, true);
}

void DatabaseDataSource::hasActiveImageSources() {
    // Check to see if there are any active image sources
    QSqlQuery query;
    query.prepare("select i.image_id \
                  from image i \
                  inner join image_source imgs on imgs.image_source_id = i.image_source_id \
                  where imgs.station_id = :stationId \
                    and i.time_stamp >= NOW() - '24 hours'::interval \
                  limit 1; ");
    query.bindValue(":stationId", getStationId());
    query.exec();

    if (query.isActive() && query.size() == 1) {
        emit activeImageSourcesAvailable();
        emit archivedImagesAvailable();
    } else {
        // No recent images but there could still be old ones...
        query.prepare("select i.image_id \
                      from image i \
                      inner join image_source imgs on imgs.image_source_id = i.image_source_id \
                      where imgs.station_id = :stationId \
                      limit 1; ");
        query.bindValue(":stationId", getStationId());
        query.exec();


        if (query.isActive() && query.size() == 1) {
            emit archivedImagesAvailable();
        }
    }
}

void DatabaseDataSource::fetchLatestImages() {
    QSqlQuery query;
    query.prepare("select i.image_id \
                  from image i \
                  inner join ( \
                      select i.image_source_id, max(i.time_stamp) as max_ts \
                      from image_source imgs \
                      inner join image i on i.image_source_id = imgs.image_source_id \
                      where imgs.station_id = :stationId \
                      group by i.image_source_id \
                  ) as x on x.image_source_id = i.image_source_id and x.max_ts = i.time_stamp \
                  where i.time_stamp >= NOW() - '24 hours'::interval");
    query.bindValue(":stationId", getStationId());
    query.setForwardOnly(true);
    bool result = query.exec();
    if (!result || !query.isActive()) {
        databaseError("fetchLatestImages", query.lastError(), query.lastQuery());
        return;
    }

    QList<int> imageIds;

    while (query.next()) {
        QSqlRecord record = query.record();
        imageIds << record.value("image_id").toInt();
    }

    fetchImages(imageIds, false);
}

void DatabaseDataSource::databaseError(QString source, QSqlError error,
                                       QString sql) {
    qDebug() << "Database Error in" << source;
    qDebug() << error.databaseText();
    qDebug() << error.driverText();
    qDebug() << error.number() << error.text() << error.type();
    qDebug() << sql;
    QString message = QString("Source: %1, Driver: %2, Database: %3").arg(
                source, error.driverText(), error.databaseText());
    QMessageBox::warning(NULL, "Database Error",
                         message);

}
