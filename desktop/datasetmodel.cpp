#include "datasetmodel.h"
#include <cmath>
#include "settings.h"

DataSetModel::DataSetModel(DataSet dataSet, SampleSet sampleSet, QMap<ExtraColumn, QString> extraColumnNames,
                           QObject *parent)
    : QAbstractTableModel(parent)
{
    if (sampleSet.reception.size() < sampleSet.timestampUnix.size()) {
        // Reception not available in the data set (not valid for this station?
        dataSet.columns.standard = dataSet.columns.standard & ~SC_Reception;
    }

    this->dataSet = dataSet;
    this->sampleSet = sampleSet;
    this->columns = getColumns();
    this->extraColumns = getExtraColumns();
    this->extraColumnNames = extraColumnNames;
}

int DataSetModel::rowCount(const QModelIndex &parent) const {
    Q_UNUSED(parent)
    return sampleSet.sampleCount;
}

int DataSetModel::columnCount(const QModelIndex &parent) const
{
    Q_UNUSED(parent)
    return columns.count() + extraColumns.count();
}

QVariant DataSetModel::data(const QModelIndex &index, int role) const
{
    if (role != Qt::DisplayRole && role != DSM_SORT_ROLE) {
        return QVariant();
    }

    if (index.column() < columns.count()) {
        StandardColumn column = columns[index.column()];
        int row = index.row();

        double value;

        switch (column) {
        case SC_Temperature:
            value = sampleSet.temperature[row];
            break;
        case SC_IndoorTemperature:
            value = sampleSet.indoorTemperature[row];
            break;
        case SC_ApparentTemperature:
            value = sampleSet.apparentTemperature[row];
            break;
        case SC_WindChill:
            value = sampleSet.windChill[row];
            break;
        case SC_DewPoint:
            value = sampleSet.dewPoint[row];
            break;
        case SC_Humidity:
            value = sampleSet.humidity[row];
            break;
        case SC_IndoorHumidity:
            value = sampleSet.indoorHumidity[row];
            break;
        case SC_Pressure:
            value = sampleSet.pressure[row];
            break;
        case SC_Rainfall:
            value = sampleSet.rainfall[row];
            break;
        case SC_AverageWindSpeed:
            value = sampleSet.averageWindSpeed[row];
            break;
        case SC_GustWindSpeed:
            value = sampleSet.gustWindSpeed[row];
            break;
        case SC_WindDirection: {
            uint idx = sampleSet.timestampUnix[row];
            if (sampleSet.windDirection.contains(idx)) {
                return sampleSet.windDirection[idx];
            }

            // Else its null.
            if (role == DSM_SORT_ROLE) {
                return QVariant(QVariant::Int);
            }
            return "--";
        }
        case SC_SolarRadiation:
            value = sampleSet.solarRadiation[row];
            break;
        case SC_UV_Index:
            value = sampleSet.uvIndex[row];
            break;
        case SC_Timestamp:
            if (role == DSM_SORT_ROLE) {
                return sampleSet.timestampUnix[row];
            }
            return QDateTime::fromTime_t(sampleSet.timestampUnix[row]);
        case SC_Reception:
            value = sampleSet.reception[row];
            break;
        case SC_HighTemperature:
            value = sampleSet.highTemperature[row];
            break;
        case SC_LowTemperature:
            value = sampleSet.lowTemperature[row];
            break;
        case SC_HighRainRate:
            value = sampleSet.highRainRate[row];
            break;
        case SC_GustWindDirection:{
            uint idx = sampleSet.timestampUnix[row];
            if (sampleSet.gustWindDirection.contains(idx)) {
                return sampleSet.gustWindDirection[idx];
            }

            // Else its null.
            if (role == DSM_SORT_ROLE) {
                return QVariant(QVariant::Int);
            }
            return "--";
        }
        case SC_Evapotranspiration:
            value = sampleSet.evapotranspiration[row];
            break;
        case SC_HighSolarRadiation:
            value = sampleSet.highSolarRadiation[row];
            break;
        case SC_HighUVIndex:
            value = sampleSet.highUVIndex[row];
            break;
        case SC_ForecastRuleId:
            return sampleSet.forecastRuleId[row];

        case SC_NoColumns:
        //default:
            // This should never happen.
            return QVariant();
        }

        if (std::isnan(value)) {
            if (role == DSM_SORT_ROLE) {
                return QVariant(QVariant::Double);
            }
            return "--";
        }

        using namespace UnitConversions;

        unit_t currentUnits = SampleColumnUnits(column);
        bool imperial = Settings::getInstance().imperial();
        bool kmh = imperial ? false : Settings::getInstance().kmh();

        switch(currentUnits) {
        case U_METERS_PER_SECOND:
            if (imperial)
                value = metersPerSecondToMilesPerHour(value);
            else if (kmh)
                value = metersPerSecondToKilometersPerHour(value);
            break;
        case U_CELSIUS:
            if (imperial)
                value = celsiusToFahrenheit(value);
            break;
        case U_HECTOPASCALS:
            if (imperial)
                value = hectopascalsToInchesOfMercury(value);
            break;
        case U_MILLIMETERS:
        case U_MILLIMETERS_PER_HOUR:
            if (imperial)
                value = millimetersToInches(value);
            break;

        case U_HUMIDITY:
        case U_WATTS_PER_SQUARE_METER:
            break; // No imperial units for this

        case U_CENTIMETERS:
        case U_VOLTAGE:
        case U_KILOMETERS_PER_HOUR:
        case U_CENTIMETERS_PER_HOUR:
            break; // These aren't base units we store data in so no conversion

        case U_MILES_PER_HOUR:
        case U_FAHRENHEIT:
        case U_INCHES_OF_MERCURY:
        case U_INCHES_PER_HOUR:
        case U_INCHES:
        case U_KNOTS:
            break; // Already in imperial units

        case U_BFT:
        case U_DAVIS_BAROMETER_TREND:
        case U_UV_INDEX:
        case U_DEGREES:
        case U_COMPASS_POINT:
            break; // These aren't metric - no unit conversions available.

        case U_UNKNOWN:
            break; // Don't know what it is - do nothing.

        case U_LEAF_WETNESS:
        case U_CENTIBAR:
            break; // These never appear for a standard column
        }

        return value;
    } else {
        ExtraColumn column = extraColumns[index.column() - columns.count()];
        int row = index.row();

        double value;

        switch (column) {
        case EC_LeafWetness1:
            value = sampleSet.leafWetness1[row];
            break;
        case EC_LeafWetness2:
            value = sampleSet.leafWetness2[row];
            break;
        case EC_LeafTemperature1:
            value = sampleSet.leafTemperature1[row];
            break;
        case EC_LeafTemperature2:
            value = sampleSet.leafTemperature2[row];
            break;
        case EC_SoilMoisture1:
            value = sampleSet.soilMoisture1[row];
            break;
        case EC_SoilMoisture2:
            value = sampleSet.soilMoisture2[row];
            break;
        case EC_SoilMoisture3:
            value = sampleSet.soilMoisture3[row];
            break;
        case EC_SoilMoisture4:
            value = sampleSet.soilMoisture4[row];
            break;
        case EC_SoilTemperature1:
            value = sampleSet.soilTemperature1[row];
            break;
        case EC_SoilTemperature2:
            value = sampleSet.soilTemperature2[row];
            break;
        case EC_SoilTemperature3:
            value = sampleSet.soilTemperature3[row];
            break;
        case EC_SoilTemperature4:
            value = sampleSet.soilTemperature4[row];
            break;
        case EC_ExtraHumidity1:
            value = sampleSet.extraHumidity1[row];
            break;
        case EC_ExtraHumidity2:
            value = sampleSet.extraHumidity2[row];
            break;
        case EC_ExtraTemperature1:
            value = sampleSet.extraTemperature1[row];
            break;
        case EC_ExtraTemperature2:
            value = sampleSet.extraTemperature2[row];
            break;
        case EC_ExtraTemperature3:
            value = sampleSet.extraTemperature3[row];
            break;
        case EC_NoColumns:
        //default:
            // This should never happen.
            return QVariant();
        }

        if (std::isnan(value)) {
            if (role == DSM_SORT_ROLE) {
                return QVariant(QVariant::Double);
            }
            return "--";
        }

        using namespace UnitConversions;

        unit_t currentUnits = SampleColumnUnits(column);
        bool imperial = Settings::getInstance().imperial();

        switch(currentUnits) {
        case U_CELSIUS:
            if (imperial)
                value = celsiusToFahrenheit(value);
            break;

        case U_HUMIDITY:
        case U_LEAF_WETNESS:
        case U_CENTIBAR:
            break; // No imperial units for this

        case U_FAHRENHEIT:
            break; // Already in imperial units

        case U_UNKNOWN:
            break; // Don't know what it is - do nothing.

        case U_METERS_PER_SECOND:
        case U_KILOMETERS_PER_HOUR:
        case U_MILES_PER_HOUR:
        case U_BFT:
        case U_KNOTS:
        case U_HECTOPASCALS:
        case U_INCHES_OF_MERCURY:
        case U_DAVIS_BAROMETER_TREND:
        case U_MILLIMETERS:
        case U_CENTIMETERS:
        case U_INCHES:
        case U_MILLIMETERS_PER_HOUR:
        case U_CENTIMETERS_PER_HOUR:
        case U_INCHES_PER_HOUR:
        case U_WATTS_PER_SQUARE_METER:
        case U_UV_INDEX:
        case U_DEGREES:
        case U_COMPASS_POINT:
        case U_VOLTAGE:
            break; // Not available as an extra column
        }

        return value;
    }
}

QVariant DataSetModel::headerData(int section, Qt::Orientation orientation, int role) const
{
    using namespace UnitConversions;
    if (role != Qt::DisplayRole) {
        return QVariant();
    }

    if (orientation == Qt::Vertical) {
        return section;
    }

    if (section < columns.count()) {
        StandardColumn column = columns[section];
        unit_t columnUnits = SampleColumnUnits(column);
        if (Settings::getInstance().imperial()) {
            columnUnits = metricToImperial(columnUnits);
        } else if (columnUnits == U_METERS_PER_SECOND && Settings::getInstance().kmh()) {
            columnUnits = U_KILOMETERS_PER_HOUR;
        }

        QString unit = unitString(columnUnits);

        switch (column) {
        case SC_Temperature:
            return tr("Temperature (%1)").arg(unit);
        case SC_IndoorTemperature:
            return tr("Indoor Temperature (%1)").arg(unit);
        case SC_ApparentTemperature:
            return tr("Apparent Temperature (%1)").arg(unit);
        case SC_WindChill:
            return tr("Wind Chill (%1)").arg(unit);
        case SC_DewPoint:
            return tr("Dew Point (%1)").arg(unit);
        case SC_Humidity:
            return tr("Humidity (%1)").arg(unit);
        case SC_IndoorHumidity:
            return tr("Indoor Humidity (%1)").arg(unit);
        case SC_Pressure:
            return tr("Pressure (%1)").arg(unit);
        case SC_Rainfall:
            return tr("Rainfall (%1)").arg(unit);
        case SC_AverageWindSpeed:
            return tr("Average Wind Speed (%1)").arg(unit);
        case SC_GustWindSpeed:
            return tr("Gust Wind Speed (%1)").arg(unit);
        case SC_WindDirection:
            return tr("Wind Direction (%1)").arg(unit);
        case SC_SolarRadiation:
            return tr("Solar Radiation (%1)").arg(unit);
        case SC_UV_Index:
            return tr("UV Index");
        case SC_Timestamp:
            return tr("Timestamp");
        case SC_Reception:
            return tr("Wireless Reception (%)");
        case SC_HighTemperature:
            return tr("High Temperature (%1)").arg(unit);
        case SC_LowTemperature:
            return tr("Low Temperature (%1)").arg(unit);
        case SC_HighRainRate:
            return tr("High Rain Rate (%1)").arg(unit);
        case SC_GustWindDirection:
            return tr("Gust Wind Direction (%1)").arg(unit);
        case SC_Evapotranspiration:
            return tr("Evapotranspiration (%1)").arg(unit);
        case SC_HighSolarRadiation:
            return tr("High Solar Radiation (%1)").arg(unit);
        case SC_HighUVIndex:
            return tr("High UV Index");
        case SC_ForecastRuleId:
            return tr("Forecast Rule ID");
        case SC_NoColumns:
        default:
            // This should never happen.
            return "?";
        }
    } else {
        ExtraColumn column = extraColumns[section - columns.count()];
        unit_t columnUnits = SampleColumnUnits(column);
        if (Settings::getInstance().imperial()) {
            columnUnits = metricToImperial(columnUnits);
        } else if (columnUnits == U_METERS_PER_SECOND && Settings::getInstance().kmh()) {
            columnUnits = U_KILOMETERS_PER_HOUR;
        }

        QString unit = unitString(columnUnits);

        // If the name has been customised:
        if (extraColumnNames.contains(column)) {
            QString name = extraColumnNames[column];
            if (column != EC_LeafWetness1 && column != EC_LeafWetness2) {
                name += " (%1)";
                return name.arg(unit);
            }
            return name;
        }

        switch (column) {
        case EC_LeafWetness1:
            return tr("Leaf Wetness 1");
        case EC_LeafWetness2:
            return tr("Leaf Wetness 2");
        case EC_LeafTemperature1:
            return tr("Leaf Temperature 1 (%1)").arg(unit);
        case EC_LeafTemperature2:
            return tr("Leaf Temperature 2 (%1)").arg(unit);
        case EC_SoilMoisture1:
            return tr("Soil Moisture 1 (%1)").arg(unit);
        case EC_SoilMoisture2:
            return tr("Soil Moisture 2 (%1)").arg(unit);
        case EC_SoilMoisture3:
            return tr("Soil Mositure 3 (%1)").arg(unit);
        case EC_SoilMoisture4:
            return tr("Soil Moisture 4 (%1)").arg(unit);
        case EC_SoilTemperature1:
            return tr("Soil Temperature 1 (%1)").arg(unit);
        case EC_SoilTemperature2:
            return tr("Soil Temperature 2 (%1)").arg(unit);
        case EC_SoilTemperature3:
            return tr("Soil Temperature 3 (%1)").arg(unit);
        case EC_SoilTemperature4:
            return tr("Soil Temperature 4 (%1)").arg(unit);
        case EC_ExtraHumidity1:
            return tr("Extra Humidity 1 (%1)").arg(unit);
        case EC_ExtraHumidity2:
            return tr("Extra Humidity 2 (%1)").arg(unit);
        case EC_ExtraTemperature1:
            return tr("Extra Temperature 1 (%1)").arg(unit);
        case EC_ExtraTemperature2:
            return tr("Extra Temperature 2 (%1)").arg(unit);
        case EC_ExtraTemperature3:
            return tr("Extra Temperature 3 (%1)").arg(unit);
        case EC_NoColumns:
        default:
            // This should never happen.
            return "?";
        }
    }

    // Likewise
    return "";
}

QList<StandardColumn> DataSetModel::getColumns() {

    SampleColumns columns = dataSet.columns;

    QList<StandardColumn> columnList;

    // Here we'll actually check data is available for the columns too. If, for
    // some reason there is no data for a column we need to exclude it from the
    // model.

    if (columns.standard.testFlag(SC_Timestamp) && !sampleSet.timestamp.isEmpty())
        columnList << SC_Timestamp;

    if (columns.standard.testFlag(SC_Temperature) && !sampleSet.temperature.isEmpty())
        columnList << SC_Temperature;

    if (columns.standard.testFlag(SC_ApparentTemperature) && !sampleSet.apparentTemperature.isEmpty())
        columnList << SC_ApparentTemperature;

    if (columns.standard.testFlag(SC_DewPoint) && !sampleSet.dewPoint.isEmpty())
        columnList << SC_DewPoint;

    if (columns.standard.testFlag(SC_WindChill) && !sampleSet.windChill.isEmpty())
        columnList << SC_WindChill;

    if (columns.standard.testFlag(SC_Humidity) && !sampleSet.humidity.isEmpty())
        columnList << SC_Humidity;

    if (columns.standard.testFlag(SC_IndoorTemperature) && !sampleSet.indoorTemperature.isEmpty())
        columnList << SC_IndoorTemperature;

    if (columns.standard.testFlag(SC_IndoorHumidity) && !sampleSet.indoorHumidity.isEmpty())
        columnList << SC_IndoorHumidity;

    if (columns.standard.testFlag(SC_Pressure) && !sampleSet.pressure.isEmpty())
        columnList << SC_Pressure;

    if (columns.standard.testFlag(SC_Rainfall) && !sampleSet.rainfall.isEmpty())
        columnList << SC_Rainfall;

    if (columns.standard.testFlag(SC_HighRainRate) && !sampleSet.highRainRate.isEmpty())
        columnList << SC_HighRainRate;

    if (columns.standard.testFlag(SC_AverageWindSpeed) && !sampleSet.averageWindSpeed.isEmpty())
        columnList << SC_AverageWindSpeed;

    // Wind direction is a dictionary keyed on timestamp rather than a vector. So its ok for
    // it to be empty.
    if (columns.standard.testFlag(SC_WindDirection))
        columnList << SC_WindDirection;

    if (columns.standard.testFlag(SC_GustWindSpeed) && !sampleSet.gustWindSpeed.isEmpty())
        columnList << SC_GustWindSpeed;

    // as above - dictionary, not vector
    if (columns.standard.testFlag(SC_GustWindDirection))
        columnList << SC_GustWindDirection;

    if (columns.standard.testFlag(SC_SolarRadiation) && !sampleSet.solarRadiation.isEmpty())
        columnList << SC_SolarRadiation;

    if (columns.standard.testFlag(SC_UV_Index) && !sampleSet.uvIndex.isEmpty())
        columnList << SC_UV_Index;

    if (columns.standard.testFlag(SC_Reception) && !sampleSet.reception.isEmpty())
        columnList << SC_Reception;

    if (columns.standard.testFlag(SC_HighTemperature) && !sampleSet.highTemperature.isEmpty())
        columnList << SC_HighTemperature;

    if (columns.standard.testFlag(SC_LowTemperature) && !sampleSet.lowTemperature.isEmpty())
        columnList << SC_LowTemperature;

    if (columns.standard.testFlag(SC_Evapotranspiration) && !sampleSet.evapotranspiration.isEmpty())
        columnList << SC_Evapotranspiration;

    if (columns.standard.testFlag(SC_HighSolarRadiation) && !sampleSet.highSolarRadiation.isEmpty())
        columnList << SC_HighSolarRadiation;

    if (columns.standard.testFlag(SC_HighUVIndex) && !sampleSet.highUVIndex.isEmpty())
        columnList << SC_HighUVIndex;

    if (columns.standard.testFlag(SC_ForecastRuleId) && !sampleSet.forecastRuleId.isEmpty())
        columnList << SC_ForecastRuleId;

    return columnList;
}

QList<ExtraColumn> DataSetModel::getExtraColumns() {

    SampleColumns columns = dataSet.columns;

    QList<ExtraColumn> columnList;

    // Here we'll actually check data is available for the columns too. If, for
    // some reason there is no data for a column we need to exclude it from the
    // model.

    if (columns.extra.testFlag(EC_LeafWetness1) && !sampleSet.leafWetness1.isEmpty())
        columnList << EC_LeafWetness1;

    if (columns.extra.testFlag(EC_LeafWetness2) && !sampleSet.leafWetness2.isEmpty())
        columnList << EC_LeafWetness2;

    if (columns.extra.testFlag(EC_LeafTemperature1) && !sampleSet.leafTemperature1.isEmpty())
        columnList << EC_LeafTemperature1;

    if (columns.extra.testFlag(EC_LeafTemperature2) && !sampleSet.leafTemperature2.isEmpty())
        columnList << EC_LeafTemperature2;

    if (columns.extra.testFlag(EC_SoilMoisture1) && !sampleSet.soilMoisture1.isEmpty())
        columnList << EC_SoilMoisture1;

    if (columns.extra.testFlag(EC_SoilMoisture2) && !sampleSet.soilMoisture2.isEmpty())
        columnList << EC_SoilMoisture2;

    if (columns.extra.testFlag(EC_SoilMoisture3) && !sampleSet.soilMoisture3.isEmpty())
        columnList << EC_SoilMoisture3;

    if (columns.extra.testFlag(EC_SoilMoisture4) && !sampleSet.soilMoisture4.isEmpty())
        columnList << EC_SoilMoisture4;

    if (columns.extra.testFlag(EC_SoilTemperature1) && !sampleSet.soilTemperature1.isEmpty())
        columnList << EC_SoilTemperature1;

    if (columns.extra.testFlag(EC_SoilTemperature2) && !sampleSet.soilTemperature2.isEmpty())
        columnList << EC_SoilTemperature2;

    if (columns.extra.testFlag(EC_SoilTemperature3) && !sampleSet.soilTemperature3.isEmpty())
        columnList << EC_SoilTemperature3;

    if (columns.extra.testFlag(EC_SoilTemperature4) && !sampleSet.soilTemperature4.isEmpty())
        columnList << EC_SoilTemperature4;

    if (columns.extra.testFlag(EC_ExtraHumidity1) && !sampleSet.extraHumidity1.isEmpty())
        columnList << EC_ExtraHumidity1;

    if (columns.extra.testFlag(EC_ExtraHumidity2) && !sampleSet.extraHumidity2.isEmpty())
        columnList << EC_ExtraHumidity2;

    if (columns.extra.testFlag(EC_ExtraTemperature1) && !sampleSet.extraTemperature1.isEmpty())
        columnList << EC_ExtraTemperature1;

    if (columns.extra.testFlag(EC_ExtraTemperature2) && !sampleSet.extraTemperature2.isEmpty())
        columnList << EC_ExtraTemperature2;

    if (columns.extra.testFlag(EC_ExtraTemperature3) && !sampleSet.extraTemperature3.isEmpty())
        columnList << EC_ExtraTemperature3;

    return columnList;
}

