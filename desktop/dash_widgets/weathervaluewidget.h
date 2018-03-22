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

    void setValue(UnitConversions::UnitValue value, SampleColumn column);
    void setOutdoorIndoorValue(
            UnitConversions::UnitValue outdoor, SampleColumn outdoorColumn,
            UnitConversions::UnitValue indoor, SampleColumn indoorColumn);
    void setDoubleValue(UnitConversions::UnitValue value1,
                        SampleColumn column1,
                        UnitConversions::UnitValue value2,
                        SampleColumn column2);
    void setName(QString name);
    void clear();

signals:
    void plotRequested(DataSet ds);

private slots:
    void showContextMenu(QPoint point);
    void plot();
    void copy();
    void toggle_kmh();

private:
    UnitConversions::UnitValue value1;
    SampleColumn column1;

    UnitConversions::UnitValue value2;
    SampleColumn column2;
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
