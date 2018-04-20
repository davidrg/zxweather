#ifndef RAINFALLWIDGET_H
#define RAINFALLWIDGET_H

#include <QWidget>

#include "datasource/abstractlivedatasource.h"
#include "datasource/sampleset.h"
#include "datasource/samplecolumns.h"
#include "charts/qcp/qcustomplot.h"

class QCustomPlot;
class QLabel;
class QFrame;
class QCPBars;
class QCPAbstractPlottable;

class RainfallWidget : public QWidget
{
    Q_OBJECT
public:
    explicit RainfallWidget(QWidget *parent = 0);
    ~RainfallWidget();

public slots:
    void liveData(LiveDataSet lds);
    void newSample(Sample sample);
    void setRain(QDate date, double day, double month, double year);
    void setStormRateEnabled(bool enabled);
    void reset();

signals:
    void chartRequested(DataSet dataset);

protected:
    void mousePressEvent(QMouseEvent *event);
    void mouseMoveEvent(QMouseEvent *event);

private slots:
    void mousePressEventSlot(QMouseEvent *event);
    void mouseMoveEventSlot(QMouseEvent *event);
    void plottableDoubleClick(QCPAbstractPlottable* plottable, int dataIndex, QMouseEvent* event);
    void showContextMenu(QPoint point);
    void plotRain();
    void save();
    void copy();

private:
    // UI
    QCustomPlot *plot;
    QLabel *label;
    QFrame *line;
    QCPBars *shortRange, *longRange;

    // Tickers
    QSharedPointer<QCPAxisTickerText> shortRangeBottomTicker;
    QSharedPointer<QCPAxisTickerText> longRangeBottomTicker;

    // Chart data
    QDate lastUpdate;
    double day, storm, rate;
    double month, year;
    bool stormRateEnabled;
    QDate stormStart;
    bool stormValid;

    // For computing changes in rainfall from changes in storm rain
    double rainExtra;
    double lastStormRain;

    void updatePlot();

    // Drag&drop support
    QString tempFileName;
    QPoint dragStartPos;
    void startDrag();

    // External plots
    void doPlot(bool shortRange, int type, bool runningTotal=true);
};

#endif // RAINFALLWIDGET_H
