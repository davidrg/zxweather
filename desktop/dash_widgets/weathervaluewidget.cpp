#include <QLabel>
#include <QHBoxLayout>

#include "weathervaluewidget.h"
#include "constants.h"

WeatherValueWidget::WeatherValueWidget(QWidget *parent) : QWidget(parent)
{
    label = new QLabel(this);

    QHBoxLayout* layout = new QHBoxLayout(this);
    layout->addWidget(label);
    layout->setSpacing(0);
    layout->setMargin(0);
    setLayout(layout);
    label->setText("--");

    insideOutside = false;
    doubleValue = false;
}

void WeatherValueWidget::setValue(
        UnitConversions::UnitValue value, SampleColumn column) {
    value1 = value;
    column1 = column;
    updateDisplay();
}

void WeatherValueWidget::setOutdoorIndoorValue(
        UnitConversions::UnitValue outdoor, SampleColumn outdoorColumn,
        UnitConversions::UnitValue indoor, SampleColumn indoorColumn) {
    value1 = outdoor;
    column1 = outdoorColumn;
    value2 = indoor;
    column2 = indoorColumn;
    insideOutside = true;
    updateDisplay();
}

void WeatherValueWidget::setDoubleValue(
        UnitConversions::UnitValue value1, SampleColumn column1,
        UnitConversions::UnitValue value2, SampleColumn column2) {
    this->value1 = value1;
    this->column1 = column1;
    this->value2 = value2;
    this->column2 = column2;
    doubleValue = true;
    updateDisplay();
}

void WeatherValueWidget::clear() {
    value1 = UnitConversions::UnitValue();
    column1 = SC_NoColumns;
    value2 = UnitConversions::UnitValue();
    column2 = SC_NoColumns;
    insideOutside = false;
    doubleValue = false;
    label->setText("--");
}

void WeatherValueWidget::updateDisplay() {
    // Barometer has davis baro trend
    // Wind speed has bft
    // Wind direction has compass point

    if (insideOutside) {
        label->setText(QString("%0 (%1 inside)")
                       .arg(QString(value1))
                       .arg(QString(value2)));
    } else if (doubleValue) {
      QString v2 = value2;
      if (v2.isEmpty()) {
          label->setText(value1);
      } else if (column1 == SC_WindDirection && column2 == SC_WindDirection) {
          // Degrees & compass point
          label->setText(QString(value1) + " " + v2);
      } else {
          label->setText(QString("%0 (%1)")
                         .arg(QString(value1)).arg(v2));
      }
    } else {
        label->setText(value1);
    }
}
