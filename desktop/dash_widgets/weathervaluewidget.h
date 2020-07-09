#ifndef WEATHERVALUEWIDGET_H
#define WEATHERVALUEWIDGET_H

#include <QWidget>

#include "unit_conversions.h"
#include "datasource/samplecolumns.h"

class QLabel;

class WeatherValueWidget : public QWidget
{
    Q_OBJECT
public:
    explicit WeatherValueWidget(QWidget *parent = NULL);

    void setValue(UnitConversions::UnitValue value, StandardColumn column);
    void setValue(UnitConversions::UnitValue value, ExtraColumn column);
    void setOutdoorIndoorValue(
            UnitConversions::UnitValue outdoor, StandardColumn outdoorColumn,
            UnitConversions::UnitValue indoor, StandardColumn indoorColumn);
    void setDoubleValue(UnitConversions::UnitValue value1,
                        StandardColumn column1,
                        UnitConversions::UnitValue value2,
                        StandardColumn column2);
    void setName(QString name);
    void clear();

signals:
    void plotRequested(DataSet ds);

private slots:
    void showContextMenu(QPoint point);
    void plot();
    void copy();
    void unitsChanged(bool imperial, bool kmh);

private:
    UnitConversions::UnitValue value1;
    StandardColumn column1;
    ExtraColumn column1ex;

    UnitConversions::UnitValue value2;
    StandardColumn column2;
    bool insideOutside;
    bool doubleValue;

    void updateDisplay();
    QLabel *label;

    // Settings
    QString name;
    bool kmh;
    bool imperial;
};

#endif // WEATHERVALUEWIDGET_H
