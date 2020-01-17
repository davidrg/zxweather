#include "databasedatasource.h"
#include "settings.h"
#ifndef NO_ECPG
#include "database.h"
#endif
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

#if (QT_VERSION < QT_VERSION_CHECK(5,0,0))
#include <limits>
#define qQNaN std::numeric_limits<double>::quiet_NaN
#endif

#include "constants.h"

DatabaseDataSource::DatabaseDataSource(AbstractProgressListener *progressListener, QObject *parent) :
    AbstractDataSource(progressListener, parent)
{
#ifndef NO_ECPG
    connect(&DBSignalAdapter::getInstance(), SIGNAL(liveDataUpdated(live_data_record)),
            this, SLOT(processLiveData(live_data_record)));
    connect(&DBSignalAdapter::getInstance(), SIGNAL(newImage(int)),
            this, SLOT(processNewImage(int)));
    connect(&DBSignalAdapter::getInstance(), SIGNAL(newSample(int)),
            this, SLOT(processNewSample(int)));
#endif

    sampleInterval = -1;
    liveDataEnabled = false;
    sensorConfigLoaded = false;
}

DatabaseDataSource::~DatabaseDataSource() {
    // Disconnect from the DB if required.
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
    if (columns.standard.testFlag(SC_Timestamp))
        query += format.arg("time_stamp");
    if (columns.standard.testFlag(SC_Temperature))
        query += format.arg("temperature");
    if (columns.standard.testFlag(SC_DewPoint))
        query += format.arg("dew_point");
    if (columns.standard.testFlag(SC_ApparentTemperature))
        query += format.arg("apparent_temperature");
    if (columns.standard.testFlag(SC_WindChill))
        query += format.arg("wind_chill");
    if (columns.standard.testFlag(SC_IndoorTemperature))
        query += format.arg("indoor_temperature");
    if (columns.standard.testFlag(SC_IndoorHumidity))
        query += format.arg("indoor_relative_humidity");
    if (columns.standard.testFlag(SC_Humidity))
        query += format.arg("relative_humidity");
    if (columns.standard.testFlag(SC_Pressure))
        query += format.arg("absolute_pressure");
    if (columns.standard.testFlag(SC_AverageWindSpeed))
        query += format.arg("average_wind_speed");
    if (columns.standard.testFlag(SC_GustWindSpeed))
        query += format.arg("gust_wind_speed");
    if (columns.standard.testFlag(SC_WindDirection))
        query += format.arg("wind_direction");
    if (columns.standard.testFlag(SC_Rainfall))
        query += format.arg("rainfall");
    if (columns.standard.testFlag(SC_UV_Index)) {
        if (qualifiers) {
            query += qualifiedFormat.arg("ds.average_uv_index");
        } else {
            query += format.arg("average_uv_index");
        }
    }
    if (columns.standard.testFlag(SC_SolarRadiation)) {
        if (qualifiers) {
            query += qualifiedFormat.arg("ds.solar_radiation");
        } else {
            query += format.arg("solar_radiation");
        }
    }
    if (columns.standard.testFlag(SC_GustWindDirection)) {
        if (qualifiers) {
            query += qualifiedFormat.arg("ds.gust_wind_direction");
        } else {
            query += format.arg("gust_wind_direction");
        }
    }
    if (columns.standard.testFlag(SC_Evapotranspiration)) {
        if (qualifiers) {
            query += qualifiedFormat.arg("ds.evapotranspiration");
        } else {
            query += format.arg("evapotranspiration");
        }
    }
    if (columns.standard.testFlag(SC_HighTemperature)) {
        if (qualifiers) {
            query += qualifiedFormat.arg("ds.high_temperature");
        } else {
            query += format.arg("high_temperature");
        }
    }
    if (columns.standard.testFlag(SC_LowTemperature)) {
        if (qualifiers) {
            query += qualifiedFormat.arg("ds.low_temperature");
        } else {
            query += format.arg("low_temperature");
        }
    }
    if (columns.standard.testFlag(SC_HighRainRate)) {
        if (qualifiers) {
            query += qualifiedFormat.arg("ds.high_rain_rate");
        } else {
            query += format.arg("high_rain_rate");
        }
    }
    if (columns.standard.testFlag(SC_HighSolarRadiation)) {
        if (qualifiers) {
            query += qualifiedFormat.arg("ds.high_solar_radiation");
        } else {
            query += format.arg("high_solar_radiation");
        }
    }
    if (columns.standard.testFlag(SC_HighUVIndex)) {
        if (qualifiers) {
            query += qualifiedFormat.arg("ds.high_uv_index");
        } else {
            query += format.arg("high_uv_index");
        }
    }
    if (columns.standard.testFlag(SC_ForecastRuleId)) {
        if (qualifiers) {
            query += qualifiedFormat.arg("ds.forecast_rule_id");
        } else {
            query += format.arg("forecast_rule_id");
        }
    }
    if (columns.standard.testFlag(SC_Reception)) {
        if (qualifiers) {
            query += qualifiedFormat.arg(
                "case when :broadcastId is null then null "
                "else round((ds.wind_sample_count / ((st.sample_interval::float) /((41+:broadcastId-1)::float /16.0 )) * 100)::numeric,1)::float "
                "end as reception");
        } else {
            query += format.arg("reception");
        }
    }
    if (columns.extra.testFlag(EC_LeafWetness1)) {
        if (qualifiers) {
            query += qualifiedFormat.arg("ds.leaf_wetness_1");
        } else {
            query += format.arg("leaf_wetness_1");
        }
    }
    if (columns.extra.testFlag(EC_LeafWetness2)) {
        if (qualifiers) {
            query += qualifiedFormat.arg("ds.leaf_wetness_2");
        } else {
            query += format.arg("leaf_wetness_2");
        }
    }
    if (columns.extra.testFlag(EC_LeafTemperature1)) {
        if (qualifiers) {
            query += qualifiedFormat.arg("ds.leaf_temperature_1");
        } else {
            query += format.arg("leaf_temperature_1");
        }
    }
    if (columns.extra.testFlag(EC_LeafTemperature2)) {
        if (qualifiers) {
            query += qualifiedFormat.arg("ds.leaf_temperature_2");
        } else {
            query += format.arg("leaf_temperature_2");
        }
    }
    if (columns.extra.testFlag(EC_SoilMoisture1)) {
        if (qualifiers) {
            query += qualifiedFormat.arg("ds.soil_moisture_1");
        } else {
            query += format.arg("soil_moisture_1");
        }
    }
    if (columns.extra.testFlag(EC_SoilMoisture2)) {
        if (qualifiers) {
            query += qualifiedFormat.arg("ds.soil_moisture_2");
        } else {
            query += format.arg("soil_moisture_2");
        }
    }
    if (columns.extra.testFlag(EC_SoilMoisture3)) {
        if (qualifiers) {
            query += qualifiedFormat.arg("ds.soil_moisture_3");
        } else {
            query += format.arg("soil_moisture_3");
        }
    }
    if (columns.extra.testFlag(EC_SoilMoisture4)) {
        if (qualifiers) {
            query += qualifiedFormat.arg("ds.soil_moisture_4");
        } else {
            query += format.arg("soil_moisture_4");
        }
    }
    if (columns.extra.testFlag(EC_SoilTemperature1)) {
        if (qualifiers) {
            query += qualifiedFormat.arg("ds.soil_temperature_1");
        } else {
            query += format.arg("soil_temperature_1");
        }
    }
    if (columns.extra.testFlag(EC_SoilTemperature2)) {
        if (qualifiers) {
            query += qualifiedFormat.arg("ds.soil_temperature_2");
        } else {
            query += format.arg("soil_temperature_2");
        }
    }
    if (columns.extra.testFlag(EC_SoilTemperature3)) {
        if (qualifiers) {
            query += qualifiedFormat.arg("ds.soil_temperature_3");
        } else {
            query += format.arg("soil_temperature_3");
        }
    }
    if (columns.extra.testFlag(EC_SoilTemperature4)) {
        if (qualifiers) {
            query += qualifiedFormat.arg("ds.soil_temperature_4");
        } else {
            query += format.arg("soil_temperature_4");
        }
    }
    if (columns.extra.testFlag(EC_ExtraHumidity1)) {
        if (qualifiers) {
            query += qualifiedFormat.arg("ds.extra_humidity_1");
        } else {
            query += format.arg("extra_humidity_2");
        }
    }
    if (columns.extra.testFlag(EC_ExtraHumidity2)) {
        if (qualifiers) {
            query += qualifiedFormat.arg("ds.extra_humidity_2");
        } else {
            query += format.arg("extra_humidity_2");
        }
    }
    if (columns.extra.testFlag(EC_ExtraTemperature1)) {
        if (qualifiers) {
            query += qualifiedFormat.arg("ds.extra_temperature_1");
        } else {
            query += format.arg("extra_temperature_1");
        }
    }
    if (columns.extra.testFlag(EC_ExtraTemperature2)) {
        if (qualifiers) {
            query += qualifiedFormat.arg("ds.extra_temperature_2");
        } else {
            query += format.arg("extra_temperature_2");
        }
    }
    if (columns.extra.testFlag(EC_ExtraTemperature3)) {
        if (qualifiers) {
            query += qualifiedFormat.arg("ds.extra_temperature_3");
        } else {
            query += format.arg("extra_temperature_3");
        }
    }


    return query;
}

QString buildSelectForColumns(SampleColumns columns)
{
    // Unset timestamp column (we'll put that in ourselves)
    columns.standard &= ~SC_Timestamp;

    QString query = "select time_stamp";
    query += buildColumnList(columns, ", %1", true, ", %1 ");

    return query;
}

QString buildGroupedSelect(SampleColumns columns, AggregateFunction function, AggregateGroupType groupType) {

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

    if (columns.standard.testFlag(SC_Timestamp))
        query += ", min(iq.time_stamp) as time_stamp ";

    // Column names in the list get wrapped in the aggregate function
    // It doesn't make sense to sum certain fields (like temperature).
    // So when AF_Sum or AF_RunningTotal is specified we'll apply that only
    // to the columns were it makes sense and select an average for all the
    // others.
    if (function == AF_Sum || function == AF_RunningTotal) {
        // Figure out which columns we can sum
        SampleColumns summables;
        summables.standard = columns.standard & SUMMABLE_COLUMNS;
        summables.extra = columns.extra & EXTRA_SUMMABLE_COLUMNS;

        // And which columns we can't
        SampleColumns nonSummables;
        nonSummables.standard = columns.standard & ~SUMMABLE_COLUMNS;
        nonSummables.standard = nonSummables.standard & ~SC_Timestamp; // we don't want timestamp either
        nonSummables.extra = columns.extra & ~EXTRA_SUMMABLE_COLUMNS;

        // Sum the summables
        if (summables.standard != SC_NoColumns || summables.extra != EC_NoColumns) {
            query += buildColumnList(summables, QString(", %1(iq.%2) as %2 ").arg(fn).arg("%1"), false);
        }

        // And just average the nonsummables(we have to apply some sort of
        // aggregate or the grouping will fail)
        if (nonSummables.standard != SC_NoColumns || nonSummables.extra != EC_NoColumns) {
            query += buildColumnList(nonSummables, ", avg(iq.%1) as %1 ", false);
        }

    } else {
        SampleColumns cols = columns;
        cols.standard = columns.standard & ~SC_Timestamp;
        query += buildColumnList(cols, QString(", %1(iq.%2) as %2 ").arg(fn).arg("%1"), false);
    }

    // Start of subquery 'iq'
    query += " from (select ";

    // Build a quadrant number to group on
    if (groupType == AGT_Custom)
        query += "(extract(epoch from cur.time_stamp) / :groupSeconds)::integer AS quadrant ";
    else if (groupType == AGT_Hour)
        query += "extract(epoch from date_trunc('hour', cur.time_stamp))::integer as quadrant ";
    else if (groupType == AGT_Day)
        query += "extract(epoch from date_trunc('day', cur.time_stamp))::integer as quadrant ";
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
    if (function == AF_RunningTotal && columns.standard.testFlag(SC_Timestamp)) {
        // This requires a window function to do it efficiently.
        // And that needs to be outside the grouped query.
        QString outer_query = "select grouped.quadrant, grouped.time_stamp ";

        // Figure out which columns we can sum
        SampleColumns summables;
        summables.standard = columns.standard & SUMMABLE_COLUMNS;
        summables.extra = columns.extra & EXTRA_SUMMABLE_COLUMNS;

        // And which columns we can't
        SampleColumns nonSummables;
        nonSummables.standard = columns.standard & ~SUMMABLE_COLUMNS;
        nonSummables.standard = nonSummables.standard & ~SC_Timestamp; // we don't want timestamp either
        nonSummables.extra = columns.extra & ~EXTRA_SUMMABLE_COLUMNS;

        // Sum the summables
        if (summables.standard != SC_NoColumns || summables.extra != EC_NoColumns) {
            outer_query += buildColumnList(summables,
                                           ", sum(grouped.%1) over (order by grouped.time_stamp) as %1 ");
        }

        // And just leave the non-summables alone.
        if (nonSummables.standard != SC_NoColumns || nonSummables.extra != EC_NoColumns) {
            outer_query += buildColumnList(nonSummables, ", grouped.%1 as %1 ", false);
        }

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

QString buildGroupedCount(AggregateFunction function, AggregateGroupType groupType) {
    SampleColumns columns;
    columns.standard = SC_NoColumns;
    columns.extra = EC_NoColumns;

    QString baseQuery = buildGroupedSelect(columns, function, groupType);

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

    QString qry = buildGroupedCount(function, groupType);
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

QSqlQuery setupBasicQuery(SampleColumns columns, int broadcastId) {

    qDebug() << "Basic Query";

    QString selectPart = buildSelectForColumns(columns);    
    selectPart += " from sample "
            " left outer join davis_sample ds on ds.sample_id = sample.sample_id "
            " inner join station st on st.station_id = sample.station_id "
            "where st.station_id = :stationId "
            "  and time_stamp >= :startTime "
            "  and time_stamp <= :endTime "
            "order by time_stamp asc";

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

    QString qry = buildGroupedSelect(columns, function, groupType);

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

double DatabaseDataSource::nullableVariantDouble(QVariant v) {
    if (v.isNull()) {
        return qQNaN();
    }
    bool ok;
    double result = v.toDouble(&ok);
    if (!ok) {
        return qQNaN();
    }
    return result;
}

int DatabaseDataSource::getSampleInterval() {
    if (sampleInterval > 0) {
        return sampleInterval;
    }

    QSqlQuery query;
    query.prepare("select sample_interval from station where station_id = :id");
    query.bindValue(":id", getStationId());
    query.exec();

    if (!query.isActive()) {
        databaseError("getSampleInterval", query.lastError(), query.lastQuery());
    }
    else if (query.size() == 1) {
        query.first();
        sampleInterval = query.value(0).toInt();
    }

    return sampleInterval;
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

    sample_range_t range = getSampleRange();
    if (range.isValid) {
        if (startTime < range.start) {
            startTime = range.start;
        }
        if (endTime > range.end) {
            endTime = range.end;
        }
    }

    if (getHardwareType() != HW_DAVIS) {
        // Turn off all the davis columns - they're not valid here
        qDebug() << "Not davis hardwrae - disabling columns";
        columns.standard = columns.standard & ~DAVIS_COLUMNS;
    }

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

    qDebug() << "Expected Row Count" << size;

    progressListener->setSubtaskName("Query...");
    progressListener->setValue(2);
    if (progressListener->wasCanceled()) return;

    int broadcastId = -1;
    if (columns.standard.testFlag(SC_Reception)) {
        // We need some extra config data for the reception column.

        QSqlQuery q;
        q.prepare("select station_config from station where station_id = :sid");
        q.bindValue(":sid", stationId);
        bool result = q.exec();
        if (!result) {
            // Couldn't get config data. Turn off the column
            qWarning() << "Failed to get station config for SC_Reception column. Errors:"
                       << q.lastError().driverText() << q.lastError().databaseText();
            columns.standard = columns.standard & ~SC_Reception;
        } else {
            q.first();
            using namespace QtJson;
            QString config = q.value(0).toString();

            bool ok;
            QVariantMap result = Json::parse(config, ok).toMap();

            if (!ok) {
                qWarning() << "Station config JSON parsing failed. Turning off reception column.";
                columns.standard = columns.standard & ~SC_Reception;
            } else if (!result["broadcast_id"].isNull()){
                broadcastId = result["broadcast_id"].toInt();
            }

        }

        if (broadcastId == -1) {
            qDebug() << "Failed to get broadcast id. Turning off reception column.";
            columns.standard = columns.standard & ~SC_Reception;
        }
    }

    int interval = -1;
    QSqlQuery query;
    if (aggregateFunction == AF_None || groupType == AGT_None) {
        query = setupBasicQuery(columns, broadcastId);
        interval = getSampleInterval();
    } else {
        SampleColumns cols;
        cols.standard = columns.standard | SC_Timestamp;
        cols.extra = columns.extra;
        query = setupGroupedQuery(cols, stationId,
                          aggregateFunction, groupType, groupMinutes,
                                  broadcastId);
        interval = groupMinutes * 60;
    }

    qDebug() <<  "Parameters: startTime -" << startTime << ", endTime -" << endTime;

    query.bindValue(":stationId", stationId);
    query.bindValue(":startTime", startTime);
    query.bindValue(":endTime", endTime);

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

    SampleSet samples;
    ReserveSampleSetSpace(samples, size, columns);

    qDebug() << "Processing results...";
    QDateTime lastTs = startTime;
    bool gapGeneration = interval > 0;
    int thresholdSeconds = 2*interval;

    qDebug() << "Gap Generation:" << gapGeneration << "Interval" << interval << "Threshold Seconds" << thresholdSeconds;

    int rowCount = 0;
    while (query.next()) {
        rowCount++;
        if (progressListener->wasCanceled()) return;

        QSqlRecord record = query.record();

        QDateTime ts = record.value("time_stamp").toDateTime();

        if (gapGeneration) {
            if (ts > lastTs.addSecs(thresholdSeconds)) {
                qDebug() << "Gap generated at: " << lastTs.addSecs(interval);
                qDebug() << "ts" << ts << "lastTs" << lastTs << "Thresh" << lastTs.addSecs(thresholdSeconds);
                // We skipped at least one sample! Generate same fake null samples.
                AppendNullSamples(samples,
                                  columns,
                                  lastTs.addSecs(interval),
                                  ts.addSecs(-1 * interval),
                                  interval);
            }
        }
        lastTs = ts;

        time_t timestamp = ts.toTime_t();
        samples.timestamp.append(timestamp);
        samples.timestampUnix.append(timestamp); // Not sure why we need both.

        if (columns.standard.testFlag(SC_Temperature))
            samples.temperature.append(nullableVariantDouble(record.value("temperature")));

        if (columns.standard.testFlag(SC_DewPoint))
            samples.dewPoint.append(nullableVariantDouble(record.value("dew_point")));

        if (columns.standard.testFlag(SC_ApparentTemperature))
            samples.apparentTemperature.append(nullableVariantDouble(
                                                   record.value("apparent_temperature")));

        if (columns.standard.testFlag(SC_WindChill))
            samples.windChill.append(nullableVariantDouble(record.value("wind_chill")));

        if (columns.standard.testFlag(SC_IndoorTemperature))
            samples.indoorTemperature.append(nullableVariantDouble(
                        record.value("indoor_temperature")));

        if (columns.standard.testFlag(SC_Humidity))
            samples.humidity.append(nullableVariantDouble(
                        record.value("relative_humidity")));

        if (columns.standard.testFlag(SC_IndoorHumidity))
            samples.indoorHumidity.append(nullableVariantDouble(
                        record.value("indoor_relative_humidity")));

        if (columns.standard.testFlag(SC_Pressure))
            samples.pressure.append(nullableVariantDouble(
                        record.value("absolute_pressure")));

        if (columns.standard.testFlag(SC_Rainfall))
            samples.rainfall.append(nullableVariantDouble(record.value("rainfall")));

        if (columns.standard.testFlag(SC_AverageWindSpeed))
            samples.averageWindSpeed.append(nullableVariantDouble(
                        record.value("average_wind_speed")));

        if (columns.standard.testFlag(SC_GustWindSpeed))
            samples.gustWindSpeed.append(nullableVariantDouble(
                        record.value("gust_wind_speed")));

        if (columns.standard.testFlag(SC_WindDirection))
            // Wind direction is often null.
            if (!record.value("wind_direction").isNull())
                samples.windDirection[timestamp] =
                        record.value("wind_direction").toUInt();

        if (columns.standard.testFlag(SC_GustWindDirection))
            // Gust wind direction is often null.
            if (!record.value("gust_wind_direction").isNull())
                samples.gustWindDirection[timestamp] =
                        record.value("gust_wind_direction").toUInt();

        if (columns.standard.testFlag(SC_UV_Index))
            samples.uvIndex.append(nullableVariantDouble(record.value("average_uv_index")));

        if (columns.standard.testFlag(SC_SolarRadiation))
            samples.solarRadiation.append(nullableVariantDouble(record.value("solar_radiation")));

        if (columns.standard.testFlag(SC_Evapotranspiration))
            samples.evapotranspiration.append(nullableVariantDouble(record.value("evapotranspiration")));

        if (columns.standard.testFlag(SC_HighTemperature))
            samples.highTemperature.append(nullableVariantDouble(record.value("high_temperature")));

        if (columns.standard.testFlag(SC_LowTemperature))
            samples.lowTemperature.append(nullableVariantDouble(record.value("low_temperature")));

        if (columns.standard.testFlag(SC_HighRainRate))
            samples.highRainRate.append(nullableVariantDouble(record.value("high_rain_rate")));

        if (columns.standard.testFlag(SC_HighSolarRadiation))
            samples.highSolarRadiation.append(nullableVariantDouble(record.value("high_solar_radiation")));

        if (columns.standard.testFlag(SC_HighUVIndex))
            samples.highUVIndex.append(nullableVariantDouble(record.value("high_uv_index")));

        if (columns.standard.testFlag(SC_Reception))
            samples.reception.append(nullableVariantDouble(record.value("reception")));

        if (columns.standard.testFlag(SC_ForecastRuleId))
            samples.forecastRuleId.append(record.value("forecast_rule_id").toInt());

        if (columns.extra.testFlag(EC_LeafWetness1))
            samples.leafWetness1.append(nullableVariantDouble(record.value("leaf_wetness_1")));

        if (columns.extra.testFlag(EC_LeafWetness2))
            samples.leafWetness2.append(nullableVariantDouble(record.value("leaf_wetness_2")));

        if (columns.extra.testFlag(EC_LeafTemperature1))
            samples.leafTemperature1.append(nullableVariantDouble(record.value("leaf_temperature_1")));

        if (columns.extra.testFlag(EC_LeafTemperature2))
            samples.leafTemperature2.append(nullableVariantDouble(record.value("leaf_temperature_2")));

        if (columns.extra.testFlag(EC_SoilMoisture1))
            samples.soilMoisture1.append(nullableVariantDouble(record.value("soil_moisture_1")));

        if (columns.extra.testFlag(EC_SoilMoisture2))
            samples.soilMoisture2.append(nullableVariantDouble(record.value("soil_moisture_2")));

        if (columns.extra.testFlag(EC_SoilMoisture3))
            samples.soilMoisture3.append(nullableVariantDouble(record.value("soil_moisture_3")));

        if (columns.extra.testFlag(EC_SoilMoisture4))
            samples.soilMoisture4.append(nullableVariantDouble(record.value("soil_moisture_4")));

        if (columns.extra.testFlag(EC_SoilTemperature1))
            samples.soilTemperature1.append(nullableVariantDouble(record.value("soil_temperature_1")));

        if (columns.extra.testFlag(EC_SoilTemperature2))
            samples.soilTemperature2.append(nullableVariantDouble(record.value("soil_temperature_2")));

        if (columns.extra.testFlag(EC_SoilTemperature3))
            samples.soilTemperature3.append(nullableVariantDouble(record.value("soil_temperature_3")));

        if (columns.extra.testFlag(EC_SoilTemperature4))
            samples.soilTemperature4.append(nullableVariantDouble(record.value("soil_temperature_4")));

        if (columns.extra.testFlag(EC_ExtraHumidity1))
            samples.extraHumidity1.append(nullableVariantDouble(record.value("extra_humidity_1")));

        if (columns.extra.testFlag(EC_ExtraHumidity2))
            samples.extraHumidity2.append(nullableVariantDouble(record.value("extra_humidity_2")));

        if (columns.extra.testFlag(EC_ExtraTemperature1))
            samples.extraTemperature1.append(nullableVariantDouble(record.value("extra_temperature_1")));

        if (columns.extra.testFlag(EC_ExtraTemperature2))
            samples.extraTemperature2.append(nullableVariantDouble(record.value("extra_temperature_2")));

        if (columns.extra.testFlag(EC_ExtraTemperature3))
            samples.extraTemperature3.append(nullableVariantDouble(record.value("extra_temperature_3")));
    }
    progressListener->setSubtaskName("Draw...");
    progressListener->setValue(4);
    if (progressListener->wasCanceled()) return;

    qDebug() << "Data retrieval complete.";

    qDebug() << "Row count:" << rowCount;
    qDebug() << "Expected samples:" << samples.sampleCount;
    qDebug() << "Have samples:" << samples.timestamp.count();
    if ((int)samples.sampleCount != samples.timestamp.count()) {
        qWarning() << "Sample count mismatch!";
    }

    emit samplesReady(samples);
    progressListener->setValue(5);
}

QSqlQuery DatabaseDataSource::query() {
    return QSqlQuery(QSqlDatabase::database());
}

#ifndef NO_ECPG
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

    // Fetching an instance will force a connect.
    DBSignalAdapter::connectInstance(target, username, password, station);
}
#endif

void DatabaseDataSource::dbError(QString message) {
    emit error(message);
}

#ifndef NO_ECPG

void DatabaseDataSource::processLiveData(live_data_record rec) {

    if (!liveDataEnabled) return;

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

    lds.davisHw.leafTemperature1 = qQNaN();
    lds.davisHw.leafTemperature2 = qQNaN();
    lds.davisHw.leafWetness1 = qQNaN();
    lds.davisHw.leafWetness2 = qQNaN();
    lds.davisHw.soilMoisture1 = qQNaN();
    lds.davisHw.soilMoisture2 = qQNaN();
    lds.davisHw.soilMoisture3 = qQNaN();
    lds.davisHw.soilMoisture4 = qQNaN();
    lds.davisHw.soilTemperature1 = qQNaN();
    lds.davisHw.soilTemperature2 = qQNaN();
    lds.davisHw.soilTemperature3 = qQNaN();
    lds.davisHw.soilTemperature4 = qQNaN();
    lds.davisHw.extraTemperature1 = qQNaN();
    lds.davisHw.extraTemperature2 = qQNaN();
    lds.davisHw.extraTemperature3 = qQNaN();
    lds.davisHw.extraHumidity1 = qQNaN();
    lds.davisHw.extraHumidity2 = qQNaN();

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

            lds.davisHw.leafWetness1 = rec.davis_data.leafWetness1;
            lds.davisHw.leafWetness2 = rec.davis_data.leafWetness2;
            lds.davisHw.leafTemperature1 = rec.davis_data.leafTemperature1;
            lds.davisHw.leafTemperature2 = rec.davis_data.leafTemperature2;
            lds.davisHw.soilMoisture1 = rec.davis_data.soilMoisture1;
            lds.davisHw.soilMoisture2 = rec.davis_data.soilMoisture2;
            lds.davisHw.soilMoisture3 = rec.davis_data.soilMoisture3;
            lds.davisHw.soilMoisture4 = rec.davis_data.soilMoisture4;
            lds.davisHw.soilTemperature1 = rec.davis_data.soilTemperature1;
            lds.davisHw.soilTemperature2 = rec.davis_data.soilTemperature2;
            lds.davisHw.soilTemperature3 = rec.davis_data.soilTemperature3;
            lds.davisHw.soilTemperature4 = rec.davis_data.soilTemperature4;
            lds.davisHw.extraTemperature1 = rec.davis_data.extraTemperature1;
            lds.davisHw.extraTemperature2 = rec.davis_data.extraTemperature2;
            lds.davisHw.extraTemperature3 = rec.davis_data.extraTemperature3;
            lds.davisHw.extraHumidity1 = rec.davis_data.extraHumidity1;
            lds.davisHw.extraHumidity2 = rec.davis_data.extraHumidity2;
        }
    }

    emit liveData(lds);
}
#endif

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
#ifndef NO_ECPG
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

    liveDataEnabled = true;

    // If we're not connected to a davis hardware force a live update immediately.
    // We do this because fine offset stations in particular update infrequently
    // (every 48 seconds) and we don't want to wait around that long to show
    // data in the UI. The data we end up outputting as a result of this will
    // correct if the station is currently online and horribly out of date if its
    // offline. Not much we can really do about that right now.
    if (getHardwareType() != HW_DAVIS) {
        processLiveData(wdb_get_live_data());
    }



#else
    emit error("Support for receiving live data from the database has not been compiled into this build of the application");
#endif
}

void DatabaseDataSource::disableLiveData() {
    liveDataEnabled = false;
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
            i.mime_type, \n\
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

    filename += QDir::separator() + QString("images") + QDir::separator() +
            stationCode.toLower() + QDir::separator() +
            imageInfo.imageSource.code.toLower() + QDir::separator() +
            imageInfo.imageTypeCode.toLower() + QDir::separator() +
            QString::number(imageInfo.timeStamp.date().year()) + QDir::separator() +
            QString::number(imageInfo.timeStamp.date().month()) + QDir::separator();

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

    return QDir::cleanPath(filename);
}

void DatabaseDataSource::fetchImages(QList<int> imageIds, bool thumbnail) {
    QStringList idList;
    foreach (int id, imageIds) {
        idList << QString::number(id);
    }
    QString idArray = "{" + idList.join(",") + "}";

    // TODO: support pulling image data from on-disk cache to make querying
    // faster. This would likely involve splitting this into two queries.

    QString qry = "select i.image_id, i.time_stamp, \
                          i.title, i.description, i.mime_type, \
                          upper(imgs.code) as src_code, imgs.source_name, \
                          upper(it.code) as image_type_code, i.metadata, \
                          it.type_name as image_type_name \
                   from image i \
                   inner join image_source imgs on imgs.image_source_id = i.image_source_id \
                   inner join image_type it on it.image_type_id = i.image_type_id \
                   where i.image_id = any(:idArray) order by time_stamp";
//i.image_data,
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

    QMap<int, ImageInfo> imageInfos;
    QMap<int, QString> cacheFiles;
    QStringList missingCacheFiles;

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

        QString filename = cacheFilename(info, stationCode);

        QFile f(filename);
        if (!f.exists()) {
            missingCacheFiles.append(QString::number(imageId));
        }

        imageInfos[imageId] = info;
        cacheFiles[imageId] = filename;
    }

    QString dataArray = "{" + missingCacheFiles.join(",") + "}";

    QSqlQuery dataQuery;
    dataQuery.prepare("select i.image_data, i.image_id from image i where i.image_id = any(:idArray)");
    dataQuery.bindValue(":idArray", dataArray);
    dataQuery.setForwardOnly(true);
    result = dataQuery.exec();
    if (!result || !dataQuery.isActive()) {
        databaseError("fetchImages", query.lastError(), query.lastQuery());
        return;
    }

    while (dataQuery.next()) {
        QSqlRecord record = dataQuery.record();
        int image_id = record.value("image_id").toInt();
        QByteArray data = record.value("image_data").toByteArray();
        QFile f(cacheFiles[image_id]);
        if (f.open(QIODevice::WriteOnly)) {
            f.write(data);
            f.close();
        }
    }

    foreach (int imageId, imageInfos.keys()) {
        ImageInfo info = imageInfos[imageId];
        QString filename = cacheFiles[imageId];
        QByteArray imageData;

        QFile file(filename);
        if (file.open(QIODevice::ReadOnly)) {
            imageData = file.readAll();
        }

        QImage srcImage;
        if (!info.mimeType.startsWith("video/")) {
            srcImage = QImage::fromData(imageData);
        }

        if (thumbnail) {
            // Don't try to thumbnail videos
            if (!info.mimeType.startsWith("video/")) {
                qDebug() << "Thumbnailing image" << imageId;
                // Resize the image
                QImage thumbnailImage = srcImage.scaled(Constants::THUMBNAIL_WIDTH,
                                                        Constants::THUMBNAIL_HEIGHT,
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

void DatabaseDataSource::fetchSamplesFromCache(DataSet dataSet) {
    AbstractDataSource::fetchSamples(dataSet);
}

void DatabaseDataSource::primeCache(QDateTime start, QDateTime end, bool imageDates) {
    Q_UNUSED(start);
    Q_UNUSED(end);
    Q_UNUSED(imageDates);
    emit cachingFinished();
}

bool DatabaseDataSource::solarAvailable() {
    using namespace QtJson;

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

            QString config = query.value(1).toString();

            bool ok;
            QVariantMap result = Json::parse(config, ok).toMap();

            if (!ok) {
                emit error("JSON parsing failed");
                return false;
            }

            if (result["has_solar_and_uv"].toBool()) {
                return true;
            }
        }
    }

    return false;
}

// Some day this will replace all the hardware type/solar available stuff outside of the
// data sources.
void DatabaseDataSource::loadSensorConfig() {
    using namespace QtJson;

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

            QString config = query.value(1).toString();

            bool ok;
            QVariantMap result = Json::parse(config, ok).toMap();

            if (!ok) {
                emit error("JSON parsing failed");
                return;
            }

            if (result["has_solar_and_uv"].toBool()) {
                sensor_config_t uv;
                uv.system_name = "uv_index";
                uv.display_name = "UV Index";
                uv.enabled = true;
                uv.isExtraColumn = false;
                uv.extraColumn = EC_NoColumns;
                uv.standardColumn = SC_UV_Index;
                sensorConfig.append(uv);

                sensor_config_t solar;
                solar.system_name = "solar_radiation";
                solar.display_name = "Solar Radiation";
                solar.enabled = true;
                solar.isExtraColumn = false;
                solar.extraColumn = EC_NoColumns;
                solar.standardColumn = SC_SolarRadiation;
                sensorConfig.append(solar);

                sensor_config_t high_uv;
                high_uv.system_name = "high_uv_index";
                high_uv.display_name = "High UV Index";
                high_uv.enabled = true;
                high_uv.isExtraColumn = false;
                high_uv.extraColumn = EC_NoColumns;
                high_uv.standardColumn = SC_HighUVIndex;
                sensorConfig.append(high_uv);

                sensor_config_t high_solar;
                high_solar.system_name = "high_solar_radiation";
                high_solar.display_name = "High Solar Radiation";
                high_solar.enabled = true;
                high_solar.isExtraColumn = false;
                high_solar.extraColumn = EC_NoColumns;
                high_solar.standardColumn = SC_HighSolarRadiation;
                sensorConfig.append(high_solar);

                sensor_config_t evapotranspiration;
                evapotranspiration.system_name = "evapotranspiration";
                evapotranspiration.display_name = "Evapotranspiration";
                evapotranspiration.enabled = true;
                evapotranspiration.isExtraColumn = false;
                evapotranspiration.extraColumn = EC_NoColumns;
                evapotranspiration.standardColumn = SC_Evapotranspiration;
                sensorConfig.append(evapotranspiration);
            }

            if (getHardwareType() == HW_DAVIS) {
                sensor_config_t high_temperature;
                high_temperature.system_name = "high_temperature";
                high_temperature.display_name = "High Temperature";
                high_temperature.enabled = true;
                high_temperature.isExtraColumn = false;
                high_temperature.extraColumn = EC_NoColumns;
                high_temperature.standardColumn = SC_HighTemperature;
                sensorConfig.append(high_temperature);

                sensor_config_t low_temperature;
                low_temperature.system_name = "low_temperature";
                low_temperature.display_name = "Low Temperature";
                low_temperature.enabled = true;
                low_temperature.isExtraColumn = false;
                low_temperature.extraColumn = EC_NoColumns;
                low_temperature.standardColumn = SC_LowTemperature;
                sensorConfig.append(low_temperature);

                sensor_config_t high_rain_rate;
                high_rain_rate.system_name = "high_rain_rate";
                high_rain_rate.display_name = "High Rain rate";
                high_rain_rate.enabled = true;
                high_rain_rate.isExtraColumn = false;
                high_rain_rate.extraColumn = EC_NoColumns;
                high_rain_rate.standardColumn = SC_HighRainRate;
                sensorConfig.append(high_rain_rate);

                sensor_config_t gust_wind_direction;
                gust_wind_direction.system_name = "gust_wind_direction";
                gust_wind_direction.display_name = "Gust Wind Direction";
                gust_wind_direction.enabled = true;
                gust_wind_direction.isExtraColumn = false;
                gust_wind_direction.extraColumn = EC_NoColumns;
                gust_wind_direction.standardColumn = SC_GustWindDirection;
                sensorConfig.append(gust_wind_direction);

                sensor_config_t forecast_rule_id;
                forecast_rule_id.system_name = "forecast_rule_id";
                forecast_rule_id.display_name = "Forecast Rule ID";
                forecast_rule_id.enabled = true;
                forecast_rule_id.isExtraColumn = false;
                forecast_rule_id.extraColumn = EC_NoColumns;
                forecast_rule_id.standardColumn = SC_ForecastRuleId;
                sensorConfig.append(forecast_rule_id);
            }

            if (result["is_wireless"].toBool()) {
                sensor_config_t wireless;
                wireless.system_name = "reception";
                wireless.display_name = "Reception";
                wireless.enabled = true;
                wireless.isExtraColumn = false;
                wireless.extraColumn = EC_NoColumns;
                wireless.standardColumn = SC_Reception;
            }

            if (result.contains("sensor_config")) {
                QVariantMap sensor_config = result["sensor_config"].toMap();

                foreach (QString key, sensor_config.keys()) {
                    QVariantMap sensor = sensor_config[key].toMap();
                    sensor_config_t config;
                    config.system_name = key;
                    config.enabled = sensor.contains("enabled") && sensor["enabled"].toBool();
                    config.isExtraColumn = false;
                    config.standardColumn = SC_NoColumns;
                    config.extraColumn = EC_NoColumns;

                    if (key == "leaf_wetness_1") {
                        config.isExtraColumn = true;
                        config.extraColumn = EC_LeafWetness1;
                        config.default_name = tr("Leaf Wetness 1");
                    } else if (key == "leaf_wetness_2") {
                        config.isExtraColumn = true;
                        config.extraColumn = EC_LeafWetness2;
                        config.default_name = tr("Leaf Wetness 2");
                    } else if (key == "leaf_temperature_1") {
                        config.isExtraColumn = true;
                        config.extraColumn = EC_LeafTemperature1;
                        config.default_name = tr("Leaf Temperature 1");
                    } else if (key == "leaf_temperature_2") {
                        config.isExtraColumn = true;
                        config.extraColumn = EC_LeafTemperature2;
                        config.default_name = tr("Leaf Temperature 2");
                    } else if (key == "soil_moisture_1") {
                        config.isExtraColumn = true;
                        config.extraColumn = EC_SoilMoisture1;
                        config.default_name = tr("Soil Moisture 1");
                    } else if (key == "soil_moisture_2") {
                        config.isExtraColumn = true;
                        config.extraColumn = EC_SoilMoisture2;
                        config.default_name = tr("Soil Moisture 2");
                    } else if (key == "soil_moisture_3") {
                        config.isExtraColumn = true;
                        config.extraColumn = EC_SoilMoisture3;
                        config.default_name = tr("Soil Moisture 3");
                    } else if (key == "soil_moisture_4") {
                        config.isExtraColumn = true;
                        config.extraColumn = EC_SoilMoisture4;
                        config.default_name = tr("Soil Moisture 4");
                    } else if (key == "soil_temperature_1") {
                        config.isExtraColumn = true;
                        config.extraColumn = EC_SoilTemperature1;
                        config.default_name = tr("Soil Temperature 1");
                    } else if (key == "soil_temperature_2") {
                        config.isExtraColumn = true;
                        config.extraColumn = EC_SoilTemperature2;
                        config.default_name = tr("Soil Temperature 2");
                    } else if (key == "soil_temperature_3") {
                        config.isExtraColumn = true;
                        config.extraColumn = EC_SoilTemperature3;
                        config.default_name = tr("Soil Temperature 3");
                    } else if (key == "soil_temperature_4") {
                        config.isExtraColumn = true;
                        config.extraColumn = EC_SoilTemperature4;
                        config.default_name = tr("Soil Temperature 4");
                    } else if (key == "extra_humidity_1") {
                        config.isExtraColumn = true;
                        config.extraColumn = EC_ExtraHumidity1;
                        config.default_name = tr("Extra Humidity 1");
                    } else if (key == "extra_humidity_2") {
                        config.isExtraColumn = true;
                        config.extraColumn = EC_ExtraHumidity2;
                        config.default_name = tr("Extra Humidity 2");
                    } else if (key == "extra_temperature_1") {
                        config.isExtraColumn = true;
                        config.extraColumn = EC_ExtraTemperature1;
                        config.default_name = tr("Extra Temperature 1");
                    } else if (key == "extra_temperature_2") {
                        config.isExtraColumn = true;
                        config.extraColumn = EC_ExtraTemperature2;
                        config.default_name = tr("Extra Temperature 2");
                    } else if (key == "extra_temperature_3") {
                        config.isExtraColumn = true;
                        config.extraColumn = EC_ExtraTemperature3;
                        config.default_name = tr("Extra Temperature 3");
                    }

                    qDebug() << "Sensor" << config.system_name
                             << "Name" << sensor["name"].toString()
                             << "Default Name" << config.default_name
                             << "Enabled" << config.enabled;

                    if (sensor.contains("name")) {
                        config.display_name = sensor["name"].toString();
                    } else {
                        config.display_name = config.default_name;
                    }

                    sensorConfig.append(config);
                }
            }
        }
    }

    sensorConfigLoaded = true;
}

ExtraColumns DatabaseDataSource::extraColumnsAvailable() {
    if (!sensorConfigLoaded) {
        loadSensorConfig();
    }

    ExtraColumns result = EC_NoColumns;

    foreach (sensor_config_t sensor, sensorConfig) {
        if (sensor.isExtraColumn && sensor.enabled) {
            result |= sensor.extraColumn;
        }
    }

    return result;
}

QMap<ExtraColumn, QString> DatabaseDataSource::extraColumnNames() {
    QMap<ExtraColumn, QString> result;

    if (!sensorConfigLoaded) {
        loadSensorConfig();
    }

    foreach (sensor_config_t sensor, sensorConfig) {
        if (sensor.isExtraColumn && sensor.enabled) {
            result[sensor.extraColumn] = sensor.display_name;
            qDebug() << sensor.display_name;
        }
    }

    return result;
}

station_info_t DatabaseDataSource::getStationInfo() {
    station_info_t info;
    info.isValid = false;

    int id = getStationId();
    qDebug() << "Get info for station" << id;

    QSqlQuery query;
    query.prepare("select stn.title, stn.description, stn.latitude, stn.longitude, stn.altitude, stn.station_config, upper(st.code) as code "
                  "from station stn inner join station_type st ON stn.station_type_id = st.station_type_id "
                  "where station_id = :id");
    query.bindValue(":id", id);
    if (query.exec()) {
        if (query.first()) {
            info.isValid = true;
            if (query.record().value("latitude").isNull() || query.record().value("longitude").isNull()) {
                qDebug() << "No coordinates present";
                info.coordinatesPresent = false;
            } else {
                info.coordinatesPresent = true;
                info.latitude = query.record().value("latitude").toFloat();
                info.longitude = query.record().value("longitude").toFloat();
                qDebug() << "lat" << info.latitude << "long" << info.longitude;
            }

            info.title = query.record().value("title").toString();
            info.description = query.record().value("description").toString();
            info.altitude = query.record().value("altitude").toFloat();
            info.isWireless = false;
            info.hasSolarAndUV = false;

            if (query.record().value("code").toString() == "DAVIS") {
                qDebug() << "Loading station config...";

                using namespace QtJson;

                QString hwConfig = query.record().value("station_config").toString();
                bool ok;
                QVariantMap result = Json::parse(hwConfig, ok).toMap();

                if (!ok) {
                    emit error("JSON parsing of station config document failed");
                    qWarning() << "Failed to parse station config";
                }
                info.hasSolarAndUV = result["has_solar_and_uv"].toBool();
                info.isWireless = result["is_wireless"].toBool();
            } else {
                qDebug() << "Not loading config for hw type" << query.record().value("code").toString();
            }
        }
    } else {
        qWarning() << "station info query failed" << query.lastError().driverText() << query.lastError().databaseText();
    }

    return info;
}

sample_range_t DatabaseDataSource::getSampleRange() {
    sample_range_t info;
    info.isValid = false;

    int id = getStationId();
    qDebug() << "Get range for station" << id;

    QSqlQuery query;
    query.prepare("select max(time_stamp) as end, min(time_stamp) as start from sample where station_id = :id");
    query.bindValue(":id", id);
    if (query.exec()) {
        if (query.first()) {
            info.start = query.record().value("start").toDateTime();
            info.end = query.record().value("end").toDateTime();
            info.isValid = true;
            return info;
        }
    }

    return info;
}
