#include "axistag.h"

AxisTag::AxisTag(QCPAxis *axis, QObject *parent) : QObject(parent)
{
    this->axis = axis;

    tracer = new QCPItemTracer(axis->parentPlot());
    tracer->setVisible(false);
    tracer->position->setTypeX(QCPItemPosition::ptAxisRectRatio);
    tracer->position->setTypeY(QCPItemPosition::ptPlotCoords);
    tracer->position->setAxisRect(axis->axisRect());
    tracer->position->setAxes(0, axis);
    tracer->position->setCoords(1, 0);

    arrow = new QCPItemLine(axis->parentPlot());
    arrow->setLayer("overlay");
    arrow->setClipAxisRect(false);
    arrow->setHead(QCPLineEnding::esSpikeArrow);
    arrow->end->setParentAnchor(tracer->position);
    arrow->start->setParentAnchor(arrow->end);
    arrow->start->setCoords(15, 0);

    label = new QCPItemText(axis->parentPlot());
    label->setLayer("overlay");
    label->setClipToAxisRect(false);
    label->setPadding(QMargins(3,0,3,0));
    label->setBrush(QBrush(Qt::white));
    label->setPen(QPen(Qt::black));
    label->setPositionAlignment(Qt::AlignLeft | Qt::AlignVCenter);
    label->position->setParentAnchor(arrow->start);
    label->setText("0.0");
}

AxisTag::~AxisTag() {
    if (tracer) {
        tracer->parentPlot()->removeItem(tracer);
    }

    if (arrow) {
        arrow->parentPlot()->removeItem(arrow);
    }

    if (label) {
        label->parentPlot()->removeItem(label);
    }
}

void AxisTag::setStyle(const GraphStyle &style) {
    label->setPen(style.getPen());
    arrow->setPen(style.getPen());
    if (style.isLive()) {
        switch (style.getLiveColumnType()) {
        case LV_WindDirection:
        case LV_Humidity:
        case LV_IndoorHumidity :
            format = 'i';
            break;

        case LV_Temperature:
        case LV_IndoorTemperature:
        case LV_ApparentTemperature:
        case LV_WindChill:
        case LV_DewPoint:
        case LV_Pressure:
        case LV_StormRain:
        case LV_RainRate:
        case LV_WindSpeed:
        case LV_UVIndex:
        case LV_SolarRadiation:
        case LV_BatteryVoltage:
        default:
            format = 'f';
            precision = 1;
        }
    } else {
        switch (style.getColumnType()) {
        case SC_WindDirection:
        case SC_Humidity:
        case SC_IndoorHumidity:
            format = 'i';
            break;

        case SC_Temperature:
        case SC_IndoorTemperature:
        case SC_ApparentTemperature:
        case SC_WindChill:
        case SC_DewPoint:
        case SC_Pressure:
        case SC_Rainfall:
        case SC_AverageWindSpeed:
        case SC_GustWindSpeed:
        case SC_UV_Index:
        case SC_SolarRadiation:
        case SC_HighTemperature:
        case SC_LowTemperature:
        case SC_HighSolarRadiation:
        case SC_HighUVIndex:
        case SC_GustWindDirection:
        case SC_HighRainRate:
        case SC_Reception:
        case SC_Evapotranspiration:
        default:
            format = 'i';
            precision = 1;
        }
    }
}


void AxisTag::setValue(double value) {
    tracer->position->setCoords(1, value);
    arrow->end->setCoords(axis->offset(), 0);
    if (format == 'i') {
        label->setText(QString::number((int)value));
    } else {
        label->setText(QString::number(value, format, precision));
    }
}
