#include "graphstyle.h"
#include "settings.h"

#include <QtDebug>

GraphStyle::GraphStyle(StandardColumn column) {
    this->column = column;
    isLiveColumn = false;
    isExtra = false;

    ChartColours colours = Settings::getInstance().getChartColours();

    QColor colour;

    scatterStyle = QCPScatterStyle::ssNone;
    brush = QBrush();
    lineStyle = QCPGraph::lsLine;


    switch (column) {
    case SC_Temperature:
        colour = colours.temperature;
        name = "Temperature";
        break;
    case SC_IndoorTemperature:
        colour = colours.indoorTemperature;
        name = "Indoor Temperature";
        break;
    case SC_ApparentTemperature:
        colour = colours.apparentTemperature;
        name = "Apparent Temperature";
        break;
    case SC_WindChill:
        colour = colours.windChill;
        name = "Wind Chill";
        break;
    case SC_DewPoint:
        colour = colours.dewPoint;
        name = "Dew Point";
        break;
    case SC_Humidity:
        colour = colours.humidity;
        name = "Humidity";
        break;
    case SC_IndoorHumidity:
        colour = colours.indoorHumidity;
        name = "Indoor Humidity";
        break;
    case SC_Pressure:
        colour = colours.pressure;
        name = "Pressure";
        break;
    case SC_Rainfall:
        colour = colours.rainfall;
        name = "Rainfall";
        lineStyle = QCPGraph::lsStepLeft;
        break;
    case SC_AverageWindSpeed:
        colour = colours.averageWindSpeed;
        name = "Average Wind Speed";
        break;
    case SC_GustWindSpeed:
        colour = colours.gustWindSpeed;
        name = "Gust Wind Speed";
        break;
    case SC_WindDirection:
        colour = colours.windDirection;
        name = "Wind Direction";
        break;
    case SC_UV_Index:
        colour = colours.uvIndex;
        name = "UV Index";
        break;
    case SC_SolarRadiation:
        colour = colours.solarRadiation;
        name = "Solar Radiation";
        break;
    case SC_HighTemperature:
        colour = colours.temperature;
        name = "High Temperature";
        break;
    case SC_LowTemperature:
        colour = colours.temperature;
        name = "Low Temperature";
        break;
    case SC_HighSolarRadiation:
        colour = colours.solarRadiation;
        name = "High Solar Radiation";
        break;
    case SC_HighUVIndex:
        colour = colours.uvIndex;
        name = "High UV Index";
        break;
    case SC_GustWindDirection:
        colour = colours.windDirection;
        name = "Gust Wind Direction";
        break;
    case SC_HighRainRate:
        colour = colours.rainfall;
        name = "High Rain Rate";
        lineStyle = QCPGraph::lsStepLeft;
        break;
    case SC_Reception:
        colour = colours.reception;
        name = "Reception";
        break;
    case SC_Evapotranspiration:
        colour = colours.evapotranspiration;
        name = "Evapotranspiration";
        lineStyle = QCPGraph::lsStepLeft;
        break;
    case SC_ForecastRuleId: // Not supported in graphs
    case SC_NoColumns:
    case SC_Timestamp:
    default:
        // This should never happen.
        colour = Qt::black;
        name = "Invalid Graph";
    }

    pen = QPen(colour);

    columnName = name;
    defaultColour = colour;

    qDebug() << "Created graph style for" << name;
}

GraphStyle::GraphStyle(ExtraColumn column) {
    this->extraColumn = column;
    isLiveColumn = false;
    isExtra = true;

    ChartColours colours = Settings::getInstance().getChartColours();

    QColor colour;

    scatterStyle = QCPScatterStyle::ssNone;
    brush = QBrush();
    lineStyle = QCPGraph::lsLine;


    switch (column) {
    case EC_LeafTemperature1:
        colour = colours.leafTemperature1;
        name = "Leaf Temperature 1";
        break;
    case EC_LeafTemperature2:
        colour = colours.leafTemperature2;
        name = "Leaf Temperature 2";
        break;
    case EC_LeafWetness1:
        colour = colours.leafWetness1;
        name = "Leaf Wetness 1";
        break;
    case EC_LeafWetness2:
        colour = colours.leafWetness2;
        name = "Leaf Wetness 2";
        break;
    case EC_SoilMoisture1:
        colour = colours.soilMoisture1;
        name = "Soil Moisture 1";
        break;
    case EC_SoilMoisture2:
        colour = colours.soilMoisture2;
        name = "Soil Moisture 2";
        break;
    case EC_SoilMoisture3:
        colour = colours.soilMoisture3;
        name = "Soil Moisture 3";
        break;
    case EC_SoilMoisture4:
        colour = colours.soilMoisture4;
        name = "Soil Moisture 4";
        break;
    case EC_SoilTemperature1:
        colour = colours.soilTemperature1;
        name = "Soil Temperature 1";
        break;
    case EC_SoilTemperature2:
        colour = colours.soilTemperature2;
        name = "Soil Temperature 2";
        break;
    case EC_SoilTemperature3:
        colour = colours.soilTemperature3;
        name = "Soil Temperature 3";
        break;
    case EC_SoilTemperature4:
        colour = colours.soilTemperature4;
        name = "Soil Temperature 4";
        break;
    case EC_ExtraHumidity1:
        colour = colours.extraHumidity1;
        name = "Extra Humidity 1";
        break;
    case EC_ExtraHumidity2:
        colour = colours.extraHumidity2;
        name = "Extra Humidity 2";
        break;
    case EC_ExtraTemperature1:
        colour = colours.extraTemperature1;
        name = "Extra Temperature 1";
        break;
    case EC_ExtraTemperature2:
        colour = colours.extraTemperature2;
        name = "Extra Temperature 2";
        break;
    case EC_ExtraTemperature3:
        colour = colours.extraTemperature3;
        name = "Extra Temperature 3";
        break;
    case EC_NoColumns:
    default:
        // This should never happen.
        colour = Qt::black;
        name = "Invalid Graph";
    }

    pen = QPen(colour);

    columnName = name;
    defaultColour = colour;

    qDebug() << "Created graph style for" << name;
}

GraphStyle::GraphStyle(LiveValue column) {
    this->liveColumn = column;
    isLiveColumn = true;
    isExtra = false;

    ChartColours colours = Settings::getInstance().getChartColours();

    QColor colour;

    scatterStyle = QCPScatterStyle::ssNone;
    brush = QBrush();
    lineStyle = QCPGraph::lsLine;

    switch (column) {
    case LV_Temperature:
        colour = colours.temperature;
        name = "Temperature";
        break;
    case LV_IndoorTemperature:
        colour = colours.indoorTemperature;
        name = "Indoor Temperature";
        break;
    case LV_ApparentTemperature:
        colour = colours.apparentTemperature;
        name = "Apparent Temperature";
        break;
    case LV_WindChill:
        colour = colours.windChill;
        name = "Wind Chill";
        break;
    case LV_DewPoint:
        colour = colours.dewPoint;
        name = "Dew Point";
        break;
    case LV_Humidity:
        colour = colours.humidity;
        name = "Humidity";
        break;
    case LV_IndoorHumidity:
        colour = colours.indoorHumidity;
        name = "Indoor Humidity";
        break;
    case LV_Pressure:
        colour = colours.pressure;
        name = "Pressure";
        break;
    case LV_StormRain:
        colour = colours.rainfall;
        name = "Storm Rain";
        lineStyle = QCPGraph::lsStepLeft;
        break;
    case LV_RainRate:
        colour = colours.rainRate;
        name = "Rain Rate";
        lineStyle = QCPGraph::lsStepLeft;
        break;
    case LV_WindSpeed:
        colour = colours.averageWindSpeed;
        name = "Average Wind Speed";
        break;
    case LV_WindDirection:
        colour = colours.windDirection;
        name = "Wind Direction";
        break;
    case LV_UVIndex:
        colour = colours.uvIndex;
        name = "UV Index";
        break;
    case LV_SolarRadiation:
        colour = colours.solarRadiation;
        name = "Solar Radiation";
        break;
    case LV_BatteryVoltage:
        colour = colours.consoleBatteryVoltage;
        name = "Console Battery Voltage";
        break;
    default:
        // This should never happen.
        colour = Qt::black;
        name = "Invalid Graph";
    }

    pen = QPen(colour);

    columnName = name;
    defaultColour = colour;

    qDebug() << "Created graph style for" << name;
}

void GraphStyle::applyStyle(QCPGraph *graph) {
    qDebug() << "Applying style for" << getName();
    graph->setName(getName());
    graph->setPen(getPen());
    graph->setScatterStyle(getScatterStyle());
    graph->setBrush(getBrush());
    graph->setLineStyle(getLineStyle());
}

void GraphStyle::setName(QString name) {
    qDebug() << "GraphStyle" << this->name << "is now" << name;
    this->name = name;
}

bool GraphStyle::operator==(GraphStyle& rhs) const {
    return (rhs.getName() == getName() &&
            rhs.getPen() == getPen() &&
            rhs.getScatterStyle().shape() == getScatterStyle().shape() &&
            rhs.getBrush() == getBrush() &&
            rhs.getLineStyle() == getLineStyle()
    );
}

bool GraphStyle::operator!=(GraphStyle& rhs) const {
    return !(operator==(rhs));
}
