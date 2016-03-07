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
    case SC_WindDirection:
        return sampleSet.windDirection[row];
    case SC_SolarRadiation:
        return sampleSet.solarRadiation[row];
    case SC_UV_Index:
        return sampleSet.uvIndex[row];
    case SC_Timestamp:
        return QDateTime::fromTime_t(sampleSet.timestampUnix[row]);

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

    if (columns.testFlag(SC_AverageWindSpeed))
        columnList << SC_AverageWindSpeed;

    if (columns.testFlag(SC_GustWindSpeed))
        columnList << SC_GustWindSpeed;

    if (columns.testFlag(SC_WindDirection))
        columnList << SC_WindDirection;

    if (columns.testFlag(SC_SolarRadiation))
        columnList << SC_SolarRadiation;

    if (columns.testFlag(SC_UV_Index))
        columnList << SC_UV_Index;

    return columnList;
}
