#include "urlhandler.h"

#include <QDate>
#include <QtDebug>
#include "datasource/abstractdatasource.h"

#include "charts/chartwindow.h"
#include "viewdatasetwindow.h"
#include "viewimageswindow.h"

#if (QT_VERSION >= QT_VERSION_CHECK(5,0,0))
#include <QUrlQuery>
#endif

UrlHandler::UrlHandler()
{

}

UrlHandler::~UrlHandler() {

}

QDate decodeDate(QUrl url) {
#if (QT_VERSION >= QT_VERSION_CHECK(5,0,0))
    QUrlQuery q(url);
    return QDate::fromString(q.queryItemValue("date"), Qt::ISODate);
#else
    return QDate::fromString(url.queryItemValue("date"), Qt::ISODate);
#endif
}

DataSet decodeDataSet(QUrl url) {
    QString start, end, title, graphs, aggregate, grouping, group_minutes;

#if (QT_VERSION >= QT_VERSION_CHECK(5,0,0))
    QUrlQuery q(url);
    start = q.queryItemValue("start");
    end = q.queryItemValue("end");
    if (q.hasQueryItem("title")) {
        title = q.queryItemValue("title");
    }
    if (q.hasQueryItem("graphs")) {
        graphs = q.queryItemValue("graphs").toLower();
    } else {
        graphs = q.queryItemValue("columns").toLower();
    }

    if (q.hasQueryItem("aggregate")) {
        aggregate = q.queryItemValue("aggregate").toLower();
        grouping = q.queryItemValue("grouping").toLower();
        if (grouping == "custom") {
            group_minutes = q.queryItemValue("group_minutes").toLower();
        }
    }
#else
    start = url.queryItemValue("start");
    end = url.queryItemValue("end");
    if (url.hasQueryItem("title")) {
        title = url.queryItemValue("title");
    }
    graphs = url.queryItemValue("graphs").toLower();
    if (url.hasQueryItem("aggregate")) {
        aggregate = url.queryItemValue("aggregate").toLower();
        grouping = url.queryItemValue("grouping").toLower();
        if (grouping == "custom") {
            group_minutes = url.queryItemValue("group_minutes").toLower();
        }
    }
#endif

    DataSet ds;
    ds.startTime = QDateTime::fromString(start, Qt::ISODate);
    ds.endTime = QDateTime::fromString(end, Qt::ISODate);
    if (!title.isNull()) {
        ds.title = title;
    }
    QStringList columns = graphs.split("+");
    foreach (QString col, columns) {
        if (col == "time")
            ds.columns.standard |= SC_Timestamp;
        if (col == "temperature") {
            ds.columns.standard |= SC_Temperature;
        } else if (col == "indoor_temperature") {
            ds.columns.standard |= SC_IndoorTemperature;
        } else if (col == "apparent_temperature") {
            ds.columns.standard |= SC_ApparentTemperature;
        } else if (col == "wind_chill") {
            ds.columns.standard |= SC_WindChill;
        } else if (col == "dew_point") {
            ds.columns.standard |= SC_DewPoint;
        } else if (col == "humidity") {
            ds.columns.standard |= SC_Humidity;
        } else if (col == "indoor_humidity") {
            ds.columns.standard |= SC_IndoorHumidity;
        } else if (col == "pressure") {
            ds.columns.standard |= SC_Pressure;
        } else if (col == "abspressure") {
            ds.columns.standard |= SC_AbsolutePressure;
        } else if (col == "mslpressure") {
            ds.columns.standard |= SC_MeanSeaLevelPressure;
        } else if (col == "rainfall") {
            ds.columns.standard |= SC_Rainfall;
        } else if (col == "average_wind_speed") {
            ds.columns.standard |= SC_AverageWindSpeed;
        } else if (col == "gust_wind_speed") {
            ds.columns.standard |= SC_GustWindSpeed;
        } else if (col == "wind_direction") {
            ds.columns.standard |= SC_WindDirection;
        } else if (col == "solar_radiation") {
            ds.columns.standard |= SC_SolarRadiation;
        } else if (col == "uv_index") {
            ds.columns.standard |= SC_UV_Index;
        } else if (col == "reception") {
            ds.columns.standard |= SC_Reception;
        } else if (col == "high_temperature") {
            ds.columns.standard |= SC_HighTemperature;
        } else if (col == "low_temperature") {
            ds.columns.standard |= SC_LowTemperature;
        } else if (col == "high_rain_rate") {
            ds.columns.standard |= SC_HighRainRate;
        } else if (col == "gust_wind_speed") {
            ds.columns.standard |= SC_GustWindDirection;
        } else if (col == "evapotranspiration") {
            ds.columns.standard |= SC_Evapotranspiration;
        } else if (col == "high_solar_radiation") {
            ds.columns.standard |= SC_HighSolarRadiation;
        } else if (col == "high_uv_index") {
            ds.columns.standard |= SC_HighUVIndex;
        } else if (col == "leaf_wetness_1") {
            ds.columns.extra |= EC_LeafWetness1;
        } else if (col == "leaf_wetness_2") {
            ds.columns.extra |= EC_LeafWetness2;
        } else if (col == "leaf_tempreature_1") {
            ds.columns.extra |= EC_LeafTemperature1;
        } else if (col == "leaf_temperature_2") {
            ds.columns.extra |= EC_LeafTemperature2;
        } else if (col == "soil_moisture_1") {
            ds.columns.extra |= EC_SoilMoisture1;
        } else if (col == "soil_moisture_2") {
            ds.columns.extra |= EC_SoilMoisture2;
        } else if (col == "soil_moisture_3") {
            ds.columns.extra |= EC_SoilMoisture3;
        } else if (col == "soil_moisture_4") {
            ds.columns.extra |= EC_SoilMoisture4;
        } else if (col == "soil_temperature_1") {
            ds.columns.extra |= EC_SoilTemperature1;
        } else if (col == "soil_temperature_2") {
            ds.columns.extra |= EC_SoilTemperature2;
        } else if (col == "soil_temperature_3") {
            ds.columns.extra |= EC_SoilTemperature3;
        } else if (col == "soil_temperature_4") {
            ds.columns.extra |= EC_SoilTemperature4;
        } else if (col == "extra_humidity_1") {
            ds.columns.extra |= EC_ExtraHumidity1;
        } else if (col == "extra_humidity_2") {
            ds.columns.extra |= EC_ExtraHumidity2;
        } else if (col == "extra_temperature_1") {
            ds.columns.extra |= EC_ExtraTemperature1;
        } else if (col == "extra_temperature_2") {
            ds.columns.extra |= EC_ExtraTemperature2;
        } else if (col == "extra_temperature_3") {
            ds.columns.extra |= EC_ExtraTemperature3;
        }
    }

    ds.aggregateFunction = AF_None;
    ds.groupType = AGT_None;
    ds.customGroupMinutes = 0;
    if (!aggregate.isNull()) {
        if (aggregate == "none") {
            ds.aggregateFunction = AF_None;
        } else if (aggregate == "average") {
            ds.aggregateFunction = AF_Average;
        } else if (aggregate == "min") {
            ds.aggregateFunction = AF_Minimum;
        } else if (aggregate == "max") {
            ds.aggregateFunction = AF_Maximum;
        } else if (aggregate == "sum") {
            ds.aggregateFunction = AF_Sum;
        } else if (aggregate == "running_total") {
            ds.aggregateFunction = AF_RunningTotal;
        }

        if (grouping == "none") {
            ds.groupType = AGT_None;
        } else if (grouping == "hour") {
            ds.groupType = AGT_Hour;
        } else if (grouping == "day") {
            ds.groupType = AGT_Day;
        } else if (grouping == "month") {
            ds.groupType = AGT_Month;
        } else if (grouping == "year") {
            ds.groupType = AGT_Year;
        } else if (grouping == "custom") {
            ds.groupType = AGT_Custom;
            ds.customGroupMinutes = group_minutes.toInt();
        }
    }
    return ds;
}

void UrlHandler::handleUrl(QUrl url, bool solarDataAvailable, bool isWireless) {
    qDebug() << "Handle URL" << url;
    if (url.authority() == "plot") {
        QList<DataSet> ds;
        ds.append(decodeDataSet(url));

        ChartWindow *cw = new ChartWindow(ds, solarDataAvailable, isWireless);
        cw->setAttribute(Qt::WA_DeleteOnClose);
        cw->show();
        return;
    } else if (url.authority() == "view-data") {
        DataSet ds = decodeDataSet(url);

        ViewDataSetWindow *window = new ViewDataSetWindow(ds);
        window->setAttribute(Qt::WA_DeleteOnClose);
        window->show();
        return;
    } else if (url.authority() == "view-images") {
        ViewImagesWindow *window = new ViewImagesWindow(decodeDate(url));
        window->setAttribute(Qt::WA_DeleteOnClose);
        window->show();
        return;
    }
}
