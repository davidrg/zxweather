#include "datasetmodel.h"

DataSetModel::DataSetModel(DataSet dataSet, SampleSet sampleSet,
                           QObject *parent)
    : QAbstractTableModel(parent)
{
    this->dataSet = dataSet;
    this->sampleSet = sampleSet;
    this->columns = getColumns();
}

int DataSetModel::rowCount(const QModelIndex &parent) const {
    return sampleSet.sampleCount;
}

int DataSetModel::columnCount(const QModelIndex &parent) const
{
    return columns.count();
}

QVariant DataSetModel::data(const QModelIndex &index, int role) const
{
    if (role != Qt::DisplayRole) {
        return QVariant();
    }

    SampleColumn column = columns[index.column()];
    int row = index.row();

    switch (column) {
    case SC_Temperature:
        return sampleSet.temperature[row];
    case SC_IndoorTemperature:
        return sampleSet.indoorTemperature[row];
    case SC_ApparentTemperature:
        return sampleSet.apparentTemperature[row];
    case SC_WindChill:
        return sampleSet.windChill[row];
    case SC_DewPoint:
        return sampleSet.dewPoint[row];
    case SC_Humidity:
        return sampleSet.humidity[row];
    case SC_IndoorHumidity:
        return sampleSet.indoorHumidity[row];
    case SC_Pressure:
        return sampleSet.pressure[row];
    case SC_Rainfall:
        return sampleSet.rainfall[row];
    case SC_AverageWindSpeed:
        return sampleSet.averageWindSpeed[row];
    case SC_GustWindSpeed:
        return sampleSet.gustWindSpeed[row];
    case SC_WindDirection: {
        uint idx = sampleSet.timestampUnix[row];
        if (sampleSet.windDirection.contains(idx)) {
            return sampleSet.windDirection[idx];
        }
        QVariant v;
        v.convert(QVariant::Int);
        return v;
    }
    case SC_SolarRadiation:
        return sampleSet.solarRadiation[row];
    case SC_UV_Index:
        return sampleSet.uvIndex[row];
    case SC_Timestamp:
        return QDateTime::fromTime_t(sampleSet.timestampUnix[row]);
    case SC_Reception:
        return sampleSet.reception[row];
    case SC_HighTemperature:
        return sampleSet.highTemperature[row];
    case SC_LowTemperature:
        return sampleSet.lowTemperature[row];
    case SC_HighRainRate:
        return sampleSet.highRainRate[row];
    case SC_GustWindDirection:{
        uint idx = sampleSet.timestampUnix[row];
        if (sampleSet.gustWindDirection.contains(idx)) {
            return sampleSet.gustWindDirection[idx];
        }
        QVariant v;
        v.convert(QVariant::Int);
        return v;
    }
    case SC_Evapotranspiration:
        return sampleSet.evapotranspiration[row];
    case SC_HighSolarRadiation:
        return sampleSet.highSolarRadiation[row];
    case SC_HighUVIndex:
        return sampleSet.highUVIndex[row];
    case SC_ForecastRuleId:
        return sampleSet.forecastRuleId[row];

    case SC_NoColumns:
    default:
        // This should never happen.
        return QVariant();
    }

    // likewise
    return QVariant();
}

QVariant DataSetModel::headerData(int section, Qt::Orientation orientation, int role) const
{
    if (role != Qt::DisplayRole) {
        return QVariant();
    }

    if (orientation == Qt::Vertical) {
        return section;
    }

    SampleColumn column = columns[section];
    switch (column) {
    case SC_Temperature:
        return tr("Temperature");
    case SC_IndoorTemperature:
        return tr("Indoor Temperature");
    case SC_ApparentTemperature:
        return tr("Apparent Temperature");
    case SC_WindChill:
        return tr("Wind Chill");
    case SC_DewPoint:
        return tr("Dew Point");
    case SC_Humidity:
        return tr("Humidity");
    case SC_IndoorHumidity:
        return tr("Indoor Humidity");
    case SC_Pressure:
        return tr("Pressure");
    case SC_Rainfall:
        return tr("Rainfall");
    case SC_AverageWindSpeed:
        return tr("Average Wind Speed");
    case SC_GustWindSpeed:
        return tr("Gust Wind Speed");
    case SC_WindDirection:
        return tr("Wind Direction");
    case SC_SolarRadiation:
        return tr("Solar Radiation");
    case SC_UV_Index:
        return tr("UV Index");
    case SC_Timestamp:
        return tr("Timestamp");
    case SC_Reception:
        return tr("Wireless Reception");
    case SC_HighTemperature:
        return tr("High Temperature");
    case SC_LowTemperature:
        return tr("Low Temperature");
    case SC_HighRainRate:
        return tr("High Rain Rate");
    case SC_GustWindDirection:
        return tr("Gust Wind Direction");
    case SC_Evapotranspiration:
        return tr("Evapotranspiration");
    case SC_HighSolarRadiation:
        return tr("High Solar Radiation");
    case SC_HighUVIndex:
        return tr("High UV Index");
    case SC_ForecastRuleId:
        return tr("Forecast Rule ID");
    case SC_NoColumns:
    default:
        // This should never happen.
        return "?";
    }

    // Likewise
    return "";
}

QList<SampleColumn> DataSetModel::getColumns() {

    SampleColumns columns = dataSet.columns;

    QList<SampleColumn> columnList;

    if (columns.testFlag(SC_Timestamp))
        columnList << SC_Timestamp;

    if (columns.testFlag(SC_Temperature))
        columnList << SC_Temperature;

    if (columns.testFlag(SC_ApparentTemperature))
        columnList << SC_ApparentTemperature;

    if (columns.testFlag(SC_DewPoint))
        columnList << SC_DewPoint;

    if (columns.testFlag(SC_WindChill))
        columnList << SC_WindChill;

    if (columns.testFlag(SC_Humidity))
        columnList << SC_Humidity;

    if (columns.testFlag(SC_IndoorTemperature))
        columnList << SC_IndoorTemperature;

    if (columns.testFlag(SC_IndoorHumidity))
        columnList << SC_IndoorHumidity;

    if (columns.testFlag(SC_Pressure))
        columnList << SC_Pressure;

    if (columns.testFlag(SC_Rainfall))
        columnList << SC_Rainfall;

    if (columns.testFlag(SC_HighRainRate))
        columnList << SC_HighRainRate;

    if (columns.testFlag(SC_AverageWindSpeed))
        columnList << SC_AverageWindSpeed;

    if (columns.testFlag(SC_WindDirection))
        columnList << SC_WindDirection;

    if (columns.testFlag(SC_GustWindSpeed))
        columnList << SC_GustWindSpeed;

    if (columns.testFlag(SC_GustWindDirection))
        columnList << SC_GustWindDirection;

    if (columns.testFlag(SC_SolarRadiation))
        columnList << SC_SolarRadiation;

    if (columns.testFlag(SC_UV_Index))
        columnList << SC_UV_Index;

    if (columns.testFlag(SC_Reception))
        columnList << SC_Reception;

    if (columns.testFlag(SC_HighTemperature))
        columnList << SC_HighTemperature;

    if (columns.testFlag(SC_LowTemperature))
        columnList << SC_LowTemperature;

    if (columns.testFlag(SC_Evapotranspiration))
        columnList << SC_Evapotranspiration;

    if (columns.testFlag(SC_HighSolarRadiation))
        columnList << SC_HighSolarRadiation;

    if (columns.testFlag(SC_HighUVIndex))
        columnList << SC_HighUVIndex;

    if (columns.testFlag(SC_ForecastRuleId))
        columnList << SC_ForecastRuleId;

    return columnList;
}
