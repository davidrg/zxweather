#ifndef CHARTWINDOW_H
#define CHARTWINDOW_H

#include <QWidget>
#include <QScopedPointer>
#include <QList>
#include <QPointer>
#include <QDateTime>
#include <QColor>

#include "chartoptionsdialog.h"
#include "datasource/webdatasource.h"
#include "qcp/qcustomplot.h"
#include "basicqcpinteractionmanager.h"
#include "weatherplotter.h"

namespace Ui {
class ChartWindow;
}

class ChartWindow : public QWidget
{
    Q_OBJECT
    
public:
    explicit ChartWindow(QList<DataSet> dataSets,
                         bool solarAvailable,
                         QWidget *parent = 0);
    ~ChartWindow();
    
private slots:
    void chartAxisCountChanged(int count);

    // chart slots
    void axisDoubleClick(QCPAxis* axis,
                         QCPAxis::SelectablePart part,
                         QMouseEvent* event);
    void titleDoubleClick(QMouseEvent*event, QCPPlotTitle*title);

    void setYAxisLock();
    void setXAxisLock();

    // Context menu related stuff
    void chartContextMenuRequested(QPoint point);
    void addTitle();
    void removeTitle(bool replot=true);
    void showLegendToggle();
    void showTitleToggle();
    void showGridToggle();
    void moveLegend();
    void removeSelectedGraph();
    void addGraph();
    void customiseChart();
    void addDataSet();

    void refresh();

    // Save As slot
    void save();

private:
    void showLegendContextMenu(QPoint point);

    QList<QCPAxis*> valueAxes();

    void reloadDataSets();

    bool gridVisible;


    Ui::ChartWindow *ui;

    void addTitle(QString title);
    QPointer<QCPPlotTitle> plotTitle;
    QString plotTitleValue;
    bool plotTitleEnabled;
    QColor plotTitleColour;

    QBrush plotBackgroundBrush;

    QScopedPointer<BasicQCPInteractionManager> basicInteractionManager;
    QScopedPointer<WeatherPlotter> plotter;

    QList<DataSet> dataSets;

    bool solarDataAvailable;
};

#endif // CHARTWINDOW_H
