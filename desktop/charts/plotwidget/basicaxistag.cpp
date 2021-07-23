#include "basicaxistag.h"
#include "charts/plotwidget.h"

BasicAxisTag::BasicAxisTag(QCPAxis* keyAxis, QCPAxis* valueAxis, bool isValueTag, bool arrow, QObject *parent):
    AbstractAxisTag(keyAxis, valueAxis, isValueTag, arrow, parent)
{

}

void BasicAxisTag::setCoords(double key, double value) {
    if (!arrow.isNull()) {
        arrow->end->setCoords(key, value);
    } else {
        label->position->setCoords(key, value);
    }
}

QPointF BasicAxisTag::coords() const {
    if (!arrow.isNull()) {
        return arrow->end->coords();
    }
    return label->position->coords();
}

void BasicAxisTag::setText(QString text) {
    label->setText(text);
}
