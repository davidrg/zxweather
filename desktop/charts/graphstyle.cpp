#include "graphstyle.h"
#include "settings.h"

#include <QtDebug>

GraphStyle::GraphStyle(SampleColumn column) {
    this->column = column;
    isLiveColumn = false;

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

GraphStyle::GraphStyle(LiveValue column) {
    this->liveColumn = column;
    isLiveColumn = true;

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
