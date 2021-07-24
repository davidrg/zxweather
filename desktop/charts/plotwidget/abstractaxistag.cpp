#include "abstractaxistag.h"
#include "charts/plotwidget.h"
#include "charts/graphstyle.h"

#include <QtDebug>

AbstractAxisTag::AbstractAxisTag(QCPAxis* keyAxis, QCPAxis* valueAxis, bool onValueAxis, bool arrow, QObject *parent): QObject(parent)
{   
    this->keyAxis = keyAxis;
    this->valueAxis = valueAxis;
    this->onValueAxis = onValueAxis;

    if (onValueAxis) {
        Q_ASSERT_X(valueAxis != NULL, "AbstractAxisTag", "Value Axis Tags must be constructed with a value axis");
    } else {
        Q_ASSERT_X(keyAxis != NULL, "AbstractAxisTag", "Key Axis Tags must be constructed with a key axis");
    }

    if (arrow) {
        this->arrow = new QCPItemLine(axis()->parentPlot());
        this->arrow->setLayer("overlay");
        this->arrow->setClipToAxisRect(false);
        this->arrow->setHead(QCPLineEnding::esSpikeArrow);
        this->arrow->start->setParentAnchor(this->arrow->end);

        switch(axis()->axisType()) {
        case QCPAxis::atLeft:
            this->arrow->start->setCoords(-15, 0);
            break;
        case QCPAxis::atRight:
            this->arrow->start->setCoords(15, 0);
            break;
        case QCPAxis::atTop:
            this->arrow->start->setCoords(0, -15);
            break;
        case QCPAxis::atBottom:
            this->arrow->start->setCoords(0, 15);
            break;
        default:
            qWarning() << "AbstractAxisTag: unrecongised axis type - unable to configure arrow!";
        }
    }


    label = new QCPItemText(axis()->parentPlot());
    label->setLayer("overlay");
    label->setClipToAxisRect(false);
    label->setPadding(QMargins(3,0,3,0));
    label->setBrush(QBrush(Qt::white));
    label->setPen(QPen(Qt::black));
    label->setText("0.0");
    label->setSelectable(false);

    if (axis()->axisType() == QCPAxis::atLeft) {
        label->setPositionAlignment(Qt::AlignRight | Qt::AlignVCenter);
    } else if (axis()->axisType() == QCPAxis::atRight) {
        label->setPositionAlignment(Qt::AlignLeft | Qt::AlignVCenter);
    } else if (axis()->axisType() == QCPAxis::atTop) {
        label->setPositionAlignment(Qt::AlignHCenter | Qt::AlignBottom);
    } else if (axis()->axisType() == QCPAxis::atBottom) {
        label->setPositionAlignment(Qt::AlignHCenter | Qt::AlignTop);
    }

    if (this->arrow) {
        label->position->setParentAnchor(this->arrow->start);
    }

    if (this->keyAxis != NULL && this->valueAxis != NULL) {
        setAxes(this->keyAxis, this->valueAxis);
    }
}

AbstractAxisTag::~AbstractAxisTag() {
    if (arrow) {
        arrow->parentPlot()->removeItem(arrow);
    }

    if (label) {
        label->parentPlot()->removeItem(label);
    }
}

void AbstractAxisTag::setAxes(QCPAxis *keyAxis, QCPAxis *valueAxis) {
    if (label) {
        label->position->setAxes(keyAxis, valueAxis);
    }

    if (arrow) {
        arrow->end->setAxes(keyAxis, valueAxis);
    }
}

void AbstractAxisTag::setVisible(bool visible) {
    if (arrow) {
        arrow->setVisible(visible);
    }
    if (label) {
        label->setVisible(visible);
    }
}

void AbstractAxisTag::setPen(const QPen &pen) {
    label->setPen(pen);

    if (arrow) {
        arrow->setPen(pen);
    }
}

void AbstractAxisTag::setStyle(const GraphStyle &style) {
    setPen(style.getPen());

    if (style.isLive()) {
        switch (style.getLiveColumnType()) {
        case LV_WindDirection:
        case LV_Humidity:
        case LV_IndoorHumidity:
        case LV_LeafWetness1:
        case LV_LeafWetness2:
        case LV_SoilMoisture1:
        case LV_SoilMoisture2:
        case LV_SoilMoisture3:
        case LV_SoilMoisture4:
        case LV_ExtraHumidity1:
        case LV_ExtraHumidity2:
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
        case LV_LeafTemperature1:
        case LV_LeafTemperature2:
        case LV_SoilTemperature1:
        case LV_SoilTemperature2:
        case LV_SoilTemperature3:
        case LV_SoilTemperature4:
        case LV_ExtraTemperature1:
        case LV_ExtraTemperature2:
        case LV_ExtraTemperature3:
        default:
            format = 'f';
            precision = 1;
        }
    } else if (style.isExtraColumn()) {
        switch(style.getExtraColumnType()) {
        case EC_LeafWetness1:
        case EC_LeafWetness2:
        case EC_SoilMoisture1:
        case EC_SoilMoisture2:
        case EC_SoilMoisture3:
        case EC_SoilMoisture4:
            format = 'i';
            break;
        case EC_LeafTemperature1:
        case EC_LeafTemperature2:
        case EC_SoilTemperature1:
        case EC_SoilTemperature2:
        case EC_SoilTemperature3:
        case EC_SoilTemperature4:
        case EC_ExtraTemperature1:
        case EC_ExtraTemperature2:
        case EC_ExtraTemperature3:
        default:
            format = 'f';
            precision = 1;
        }
    }else {
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

QFont AbstractAxisTag::font() {
    return label->font();
}

QString AbstractAxisTag::text() {
    return label->text();
}

QCPAxis* AbstractAxisTag::axis() {
    return onValueAxis ? valueAxis.data() : keyAxis.data();
}
