#ifndef RAINFALLWIDGET_H
#define RAINFALLWIDGET_H

#include <QWidget>

#include "datasource/abstractlivedatasource.h"
#include "datasource/sampleset.h"

class QCustomPlot;
class QLabel;
class QFrame;
class QCPBars;

class RainfallWidget : public QWidget
{
    Q_OBJECT
public:
    explicit RainfallWidget(QWidget *parent = 0);

signals:

public slots:
    void liveData(LiveDataSet lds);
    void newSample(Sample sample);
    void setRain(QDate date, double day, double month, double year);
    void setStormRateEnabled(bool enabled);
    void reset();

private:
    // UI
    QCustomPlot *plot;
    QLabel *label;
    QFrame *line;
    QCPBars *shortRange, *longRange;

    // Chart data
    QDate lastUpdate;
    double day, storm, rate;
    double month, year;
    bool stormRateEnabled;

    void updatePlot();
};

#endif // RAINFALLWIDGET_H
