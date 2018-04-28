#include "datasetmodel.h"

DataSetModel::DataSetModel(DataSet dataSet, SampleSet sampleSet,
                           QObject *parent)
    : QAbstractTableModel(parent)
{
    if (sampleSet.reception.size() < sampleSet.timestampUnix.size()) {
        // Reception not available in the data set (not valid for this station?
        dataSet.columns = dataSet.columns & ~SC_Reception;
    }

    this->dataSet = dataSet;
    this->sampleSet = sampleSet;
    this->columns = getColumns();
}

int DataSetModel::rowCount(const QModelIndex &parent) const {
    Q_UNUSED(parent)
    return sampleSet.sampleCount;
}

int DataSetModel::columnCount(const QModelIndex &parent) const
{
    Q_UNUSED(parent)
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

    // Here we'll actually check data is available for the columns too. If, for
    // some reason there is no data for a column we need to exclude it from the
    // model.

    if (columns.testFlag(SC_Timestamp) && !sampleSet.timestamp.isEmpty())
        columnList << SC_Timestamp;

    if (columns.testFlag(SC_Temperature) && !sampleSet.temperature.isEmpty())
        columnList << SC_Temperature;

    if (columns.testFlag(SC_ApparentTemperature) && !sampleSet.apparentTemperature.isEmpty())
        columnList << SC_ApparentTemperature;

    if (columns.testFlag(SC_DewPoint) && !sampleSet.dewPoint.isEmpty())
        columnList << SC_DewPoint;

    if (columns.testFlag(SC_WindChill) && !sampleSet.windChill.isEmpty())
        columnList << SC_WindChill;

    if (columns.testFlag(SC_Humidity) && !sampleSet.humidity.isEmpty())
        columnList << SC_Humidity;

    if (columns.testFlag(SC_IndoorTemperature) && !sampleSet.indoorTemperature.isEmpty())
        columnList << SC_IndoorTemperature;

    if (columns.testFlag(SC_IndoorHumidity) && !sampleSet.indoorHumidity.isEmpty())
        columnList << SC_IndoorHumidity;

    if (columns.testFlag(SC_Pressure) && !sampleSet.pressure.isEmpty())
        columnList << SC_Pressure;

    if (columns.testFlag(SC_Rainfall) && !sampleSet.rainfall.isEmpty())
        columnList << SC_Rainfall;

    if (columns.testFlag(SC_HighRainRate) && !sampleSet.highRainRate.isEmpty())
        columnList << SC_HighRainRate;

    if (columns.testFlag(SC_AverageWindSpeed) && !sampleSet.averageWindSpeed.isEmpty())
        columnList << SC_AverageWindSpeed;

    // Wind direction is a dictionary keyed on timestamp rather than a vector. So its ok for
    // it to be empty.
    if (columns.testFlag(SC_WindDirection))
        columnList << SC_WindDirection;

    if (columns.testFlag(SC_GustWindSpeed) && !sampleSet.gustWindSpeed.isEmpty())
        columnList << SC_GustWindSpeed;

    // as above - dictionary, not vector
    if (columns.testFlag(SC_GustWindDirection))
        columnList << SC_GustWindDirection;

    if (columns.testFlag(SC_SolarRadiation) && !sampleSet.solarRadiation.isEmpty())
        columnList << SC_SolarRadiation;

    if (columns.testFlag(SC_UV_Index) && !sampleSet.uvIndex.isEmpty())
        columnList << SC_UV_Index;

    if (columns.testFlag(SC_Reception) && !sampleSet.reception.isEmpty())
        columnList << SC_Reception;

    if (columns.testFlag(SC_HighTemperature) && !sampleSet.highTemperature.isEmpty())
        columnList << SC_HighTemperature;

    if (columns.testFlag(SC_LowTemperature) && !sampleSet.lowTemperature.isEmpty())
        columnList << SC_LowTemperature;

    if (columns.testFlag(SC_Evapotranspiration) && !sampleSet.evapotranspiration.isEmpty())
        columnList << SC_Evapotranspiration;

    if (columns.testFlag(SC_HighSolarRadiation) && !sampleSet.highSolarRadiation.isEmpty())
        columnList << SC_HighSolarRadiation;

    if (columns.testFlag(SC_HighUVIndex) && !sampleSet.highUVIndex.isEmpty())
        columnList << SC_HighUVIndex;

    if (columns.testFlag(SC_ForecastRuleId) && !sampleSet.forecastRuleId.isEmpty())
        columnList << SC_ForecastRuleId;

    return columnList;
}
