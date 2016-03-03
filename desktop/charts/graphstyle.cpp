#include "graphstyle.h"
#include "settings.h"

#include <QtDebug>

GraphStyle::GraphStyle(SampleColumn column) {
    this->column = column;

    ChartColours colours = Settings::getInstance().getChartColours();

    QColor colour;

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
    case SC_NoColumns:
    case SC_Timestamp:
    default:
        // This should never happen.
        colour = Qt::black;
        name = "Invalid Graph";
    }

    columnName = name;
    defaultColour = colour;

    pen = QPen(colour);
    scatterStyle = QCPScatterStyle::ssNone;
    brush = QBrush();
    lineStyle = QCPGraph::lsLine;

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
