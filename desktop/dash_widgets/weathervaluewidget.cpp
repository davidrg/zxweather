#include <QLabel>
#include <QVBoxLayout>

#include "weathervaluewidget.h"
#include "constants.h"

WeatherValueWidget::WeatherValueWidget(QWidget *parent) : QWidget(parent)
{
    label = new QLabel(this);

    QVBoxLayout* layout = new QVBoxLayout(this);
    layout->addWidget(label);
    setLayout(layout);
}

void WeatherValueWidget::setValue(
        UnitConversions::UnitValue value, SampleColumn column) {
    outdoorValue = value;
    outdoorColumn = column;
}

void WeatherValueWidget::setOutdoorIndoorValue(
        UnitConversions::UnitValue outdoor, SampleColumn outdoorColumn,
        UnitConversions::UnitValue indoor, SampleColumn indoorColumn) {
    setValue(outdoor, outdoorColumn);
    indoorValue = indoor;
    indoorColumn = indoorColumn;
    insideOutside = true;
}

void WeatherValueWidget::updateDisplay() {
    if (insideOutside) {
        label->setText(QString("%0 (%1 inside)")
                       .arg(QString(outdoorValue))
                       .arg(QString(indoorValue)));
    } else {
        label->setText(outdoorValue);
    }
}
