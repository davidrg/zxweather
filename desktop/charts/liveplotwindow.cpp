#include "liveplotwindow.h"
#include "ui_liveplotwindow.h"

#include <QtDebug>

#include "datasource/abstractlivedatasource.h"
#include "datasource/databasedatasource.h"
#include "datasource/webdatasource.h"
#include "datasource/tcplivedatasource.h"
#include "datasource/dialogprogresslistener.h"
#include "qcp/qcustomplot.h"
#include "settings.h"
#include "constants.h"
#include "addlivegraphdialog.h"
#include "nonaggregatingliveaggregator.h"
#include "averagedliveaggregator.h"
#include "livechartoptionsdialog.h"


#define PROP_GRAPH_TYPE "graph_type"

LivePlotWindow::LivePlotWindow(bool solarAvailalble, hardware_type_t hardwareType, QWidget *parent) :
    QMainWindow(parent),
    ui(new Ui::LivePlotWindow)
{
    ui->setupUi(this);

    this->hwType = hardwareType;
    this->solarAvailable = solarAvailalble;

    showAddGraphDialog(
                tr("Select the values to display in the live chart. More can be added "
                   "later."),
                tr("Choose graphs"));

    // All the possible axis types
    units[LV_Temperature] = UnitConversions::U_CELSIUS;
    units[LV_IndoorTemperature] = UnitConversions::U_CELSIUS;
    units[LV_ApparentTemperature] = UnitConversions::U_CELSIUS;
    units[LV_WindChill] = UnitConversions::U_CELSIUS;
    units[LV_DewPoint] = UnitConversions::U_CELSIUS;
    units[LV_Humidity] = UnitConversions::U_HUMIDITY;
    units[LV_IndoorHumidity] = UnitConversions::U_HUMIDITY;
    units[LV_Pressure] = UnitConversions::U_HECTOPASCALS;
    units[LV_WindSpeed] = UnitConversions::U_METERS_PER_SECOND;
    units[LV_WindDirection] = UnitConversions::U_DEGREES;
    units[LV_StormRain] = UnitConversions::U_MILLIMETERS;
    units[LV_RainRate] = UnitConversions::U_MILLIMETERS_PER_HOUR;
    units[LV_BatteryVoltage] = UnitConversions::U_VOLTAGE;
    units[LV_UVIndex] = UnitConversions::U_UV_INDEX;
    units[LV_SolarRadiation] = UnitConversions::U_WATTS_PER_SQUARE_METER;

    // And their labels
    axisLabels[UnitConversions::U_CELSIUS] = tr("Temperature (" DEGREE_SYMBOL "C)");
    axisLabels[UnitConversions::U_HUMIDITY] = tr("Humidity (%)");
    axisLabels[UnitConversions::U_HECTOPASCALS] = tr("Pressure (hPa)");
    axisLabels[UnitConversions::U_METERS_PER_SECOND] = tr("Wind Speed (m/s)");
    axisLabels[UnitConversions::U_DEGREES] = tr("Wind direction (" DEGREE_SYMBOL ")");
    axisLabels[UnitConversions::U_MILLIMETERS] = tr("Rainfall (mm)");
    axisLabels[UnitConversions::U_MILLIMETERS_PER_HOUR] = tr("Rain Rate (mm/h");
    axisLabels[UnitConversions::U_VOLTAGE] = tr("Voltage (V)");
    axisLabels[UnitConversions::U_UV_INDEX] = tr("UV Index");
    axisLabels[UnitConversions::U_WATTS_PER_SQUARE_METER] = tr("Solar Radiation (W/m" SQUARED_SYMBOL ")");

    Settings &settings = Settings::getInstance();

    switch(settings.liveDataSourceType()) {
    case Settings::DS_TYPE_DATABASE:
        ds = new DatabaseDataSource(new DialogProgressListener(this), this);
        break;
    case Settings::DS_TYPE_SERVER:
        ds = new TcpLiveDataSource(this);
        break;
    case Settings::DS_TYPE_WEB_INTERFACE:
        ds = new WebDataSource(new DialogProgressListener(this), this);
        break;
    }

    // This is to give the averaging aggregator a constant stream of updates
    // so it keeps producing new samples when the weater station goes quiet.
    repeater.reset(new LiveDataRepeater(this));
    connect(ds, SIGNAL(liveData(LiveDataSet)), repeater.data(), SLOT(incomingLiveData(LiveDataSet)));

    // Setup the aggregator
    aggregateSeconds = settings.liveAggregateSeconds();
    aggregate = settings.liveAggregate();
    maxRainRate = settings.liveMaxRainRate();
    stormRain = settings.liveStormRain();
    timespanMinutes = settings.liveTimespanMinutes();

    if (aggregate) {
        aggregator.reset(new AveragedLiveAggregator(aggregateSeconds, maxRainRate, stormRain, this));
    } else {
        aggregator.reset(new NonAggregatingLiveAggregator(stormRain, this));
    }

    connect(repeater.data(), SIGNAL(liveData(LiveDataSet)),
            aggregator.data(), SLOT(incomingLiveData(LiveDataSet)));
    connect(aggregator.data(), SIGNAL(liveData(LiveDataSet)),
            this, SLOT(liveData(LiveDataSet)));

    ds->enableLiveData();

    // Configure the plot
    QSharedPointer<QCPAxisTicker> ticker(new QCPAxisTickerDateTime());
    ui->plot->xAxis->setTicker(ticker);
    ui->plot->yAxis->setVisible(false);
    ui->plot->yAxis->setTickLabels(false);

    ui->plot->setBackground(QBrush(Settings::getInstance().getChartColours().background));

    // Hookup toolbar buttons
    connect(ui->actionAdd_Graph, SIGNAL(triggered(bool)),
            this, SLOT(showAddGraphDialog()));
    connect(ui->actionSave, SIGNAL(triggered(bool)),
            this->ui->plot, SLOT(save()));
    connect(ui->actionCopy, SIGNAL(triggered(bool)),
            this->ui->plot, SLOT(copy()));
    connect(ui->actionRemove_Graph, SIGNAL(triggered(bool)),
            ui->plot, SLOT(removeSelectedGraph()));
    connect(ui->actionLegend, SIGNAL(triggered(bool)),
            ui->plot, SLOT(toggleLegend()));
    connect(ui->actionTitle, SIGNAL(triggered(bool)),
            ui->plot, SLOT(toggleTitle()));
    connect(ui->actionOptions, SIGNAL(triggered(bool)),
            this, SLOT(showOptions()));

    // Events from the plotting widget
    connect(ui->plot, SIGNAL(addGraphRequested()),
            this, SLOT(showAddGraphDialog()));
    connect(ui->plot, SIGNAL(removingGraph(QCPGraph*)),
            this, SLOT(graphRemoving(QCPGraph*)));
    connect(ui->plot, SIGNAL(selectionChangedByUser()),
            this, SLOT(selectionChanged()));
    connect(ui->plot, SIGNAL(titleVisibilityChanged(bool)),
            ui->actionTitle, SLOT(setChecked(bool)));
    connect(ui->plot, SIGNAL(legendVisibilityChanged(bool)),
            ui->actionLegend, SLOT(setChecked(bool)));
}

LivePlotWindow::~LivePlotWindow()
{
    delete ui;
}

LiveValues LivePlotWindow::liveValues() {
    return valuesToShow;
}

void LivePlotWindow::addLiveValue(LiveValue v) {
    valuesToShow |= v;

    UnitConversions::unit_t axisType = units[v];

    if (!axis.contains(axisType)) {
        bool isLeft = axis.count() % 2 == 0;

        if(!ui->plot->yAxis->visible()) {
            qDebug() << "Using Y1";
            axis[axisType] = ui->plot->yAxis;
            ui->plot->yAxis->setVisible(true);
        } else if (!ui->plot->yAxis2->visible()) {
            qDebug() << "Using Y2";
            axis[axisType] = ui->plot->yAxis2;
            ui->plot->yAxis2->setVisible(true);
        } else {
            axis[axisType] = ui->plot->axisRect()->addAxis(
                        isLeft ? QCPAxis::atLeft : QCPAxis::atRight);
        }
        axis[axisType]->setVisible(true);
        axis[axisType]->setTickLabels(true);
        axis[axisType]->setLabel(axisLabels[axisType]);
    }

    if (!graphs.contains(v)) {
        ChartColours colours = Settings::getInstance().getChartColours();

        graphs[v] = ui->plot->addStyledGraph(ui->plot->xAxis, axis[axisType],
                                             GraphStyle(v));
        graphs[v]->setProperty(PROP_GRAPH_TYPE, v);

        points[v] = new QCPGraph(ui->plot->xAxis, axis[axisType]);

        points[v]->setLineStyle(QCPGraph::lsNone);
        points[v]->setScatterStyle(QCPScatterStyle::ssDisc);
        points[v]->removeFromLegend();

        switch(v) {
        case LV_Temperature:
            graphs[v]->setName(tr("Temperature"));
            graphs[v]->setPen(QPen(colours.temperature));
            break;
        case LV_IndoorTemperature:
            graphs[v]->setName(tr("Indoor Temperature"));
            graphs[v]->setPen(QPen(colours.indoorTemperature));
            break;
        case LV_ApparentTemperature:
            graphs[v]->setName(tr("Apparent Temperature"));
            graphs[v]->setPen(QPen(colours.apparentTemperature));
            break;
        case LV_WindChill:
            graphs[v]->setName(tr("Wind Chill"));
            graphs[v]->setPen(QPen(colours.windChill));
            break;
        case LV_DewPoint:
            graphs[v]->setName(tr("Dew Point"));
            graphs[v]->setPen(QPen(colours.dewPoint));
            break;
        case LV_Humidity:
            graphs[v]->setName(tr("Humidity"));
            graphs[v]->setPen(QPen(colours.humidity));
            break;
        case LV_IndoorHumidity:
            graphs[v]->setName(tr("Indoor Humidity"));
            graphs[v]->setPen(QPen(colours.indoorHumidity));
            break;
        case LV_Pressure:
            graphs[v]->setName(tr("Pressure"));
            graphs[v]->setPen(QPen(colours.pressure));
            break;
        case LV_WindSpeed:
            graphs[v]->setName(tr("Wind Speed"));
            graphs[v]->setPen(QPen(colours.averageWindSpeed));
            break;
        case LV_WindDirection:
            graphs[v]->setName(tr("Wind Direction"));
            graphs[v]->setPen(QPen(colours.windDirection));
            break;
        case LV_StormRain:
            graphs[v]->setName(tr("Storm Rain"));
            graphs[v]->setPen(QPen(colours.rainfall));
            break;
        case LV_RainRate:
            graphs[v]->setName(tr("Rain Rate"));
            graphs[v]->setPen(QPen(colours.rainRate));
            break;
        case LV_BatteryVoltage:
            graphs[v]->setName(tr("Battery Voltage"));
            graphs[v]->setPen(QPen(colours.consoleBatteryVoltage));
            break;
        case LV_UVIndex:
            graphs[v]->setName(tr("UV Index"));
            graphs[v]->setPen(QPen(colours.uvIndex));
            break;
        case LV_SolarRadiation:
            graphs[v]->setName(tr("Solar Radiation"));
            graphs[v]->setPen(QPen(colours.solarRadiation));
            break;
        case LV_NoColumns:
        default:
            break; // nothing to do.
        }

        points[v]->setPen(graphs[v]->pen());
        points[v]->setBrush(graphs[v]->pen().color());

        ui->plot->legend->setVisible(graphs.count() > 1);
        ui->actionLegend->setChecked(ui->plot->legend->visible());
    }

    ui->plot->replot();
}

void LivePlotWindow::liveData(LiveDataSet ds) {
    double ts = ds.timestamp.toMSecsSinceEpoch() / 1000.0;

    qint64 xRange = timespanMinutes * 60; // seconds
    double padding = 0.5 * timespanMinutes; // TempView uses 1.0 for 2 minutes, 100.0 for 2 hours.

    // Scroll the key axis
    ui->plot->xAxis->setRange(ts + padding,
                              xRange,
                              Qt::AlignRight);

    updateGraph(LV_Temperature, ts, xRange, ds.temperature);
    updateGraph(LV_IndoorTemperature, ts, xRange, ds.indoorTemperature);
    updateGraph(LV_ApparentTemperature, ts, xRange, ds.apparentTemperature);
    updateGraph(LV_WindChill, ts, xRange, ds.windChill);
    updateGraph(LV_DewPoint, ts, xRange, ds.dewPoint);
    updateGraph(LV_Humidity, ts, xRange, ds.humidity);
    updateGraph(LV_IndoorHumidity, ts, xRange, ds.indoorHumidity);
    updateGraph(LV_Pressure, ts, xRange, ds.pressure);
    updateGraph(LV_WindSpeed, ts, xRange, ds.windSpeed);
    updateGraph(LV_WindDirection, ts, xRange, ds.windDirection);
    updateGraph(LV_StormRain, ts, xRange, ds.davisHw.stormRain);
    updateGraph(LV_RainRate, ts, xRange, ds.davisHw.rainRate);
    updateGraph(LV_BatteryVoltage, ts, xRange, ds.davisHw.consoleBatteryVoltage);
    updateGraph(LV_UVIndex, ts, xRange, ds.davisHw.uvIndex);
    updateGraph(LV_SolarRadiation, ts, xRange, ds.davisHw.solarRadiation);

    ui->plot->replot();
}

void LivePlotWindow::updateGraph(LiveValue type, double key, double range, double value) {
    if (graphs.contains(type)) {
        graphs[type]->data()->removeBefore(key - range);
        graphs[type]->addData(key, value);
        points[type]->data()->clear();
        points[type]->addData(key, value);

        QCPRange oldRange = graphs[type]->valueAxis()->range();
        graphs[type]->rescaleValueAxis();

        // Add a bit of padding to the Y axis - the range tends to be relatively small
        // and often you can end up with the line just following the very top and
        // bottom of the automatic range.
        QCPRange range = graphs[type]->valueAxis()->range();

        // But only apply padding if the range changed during the rescale. Otherwise we
        // just cause the axes range to slowly drift larger
        if (range.lower != oldRange.lower || range.upper != oldRange.upper) {
            graphs[type]->valueAxis()->setRange(range.lower - 1.0,
                                                range.upper + 1.0);
        }

        // For all value axes except temperature ensure they don't go below zero.
        // Because a negative rain rate would be concerning.
        if (units[type] != UnitConversions::U_CELSIUS
                && units[type] != UnitConversions::U_FAHRENHEIT) {
            if (graphs[type]->valueAxis()->range().lower < 0) {
                graphs[type]->valueAxis()->setRangeLower(0);
            }
        }
    }
}

#define TEST_ADD_COL(col) if(columns.testFlag(col)) addLiveValue(col)

void LivePlotWindow::showAddGraphDialog(QString message, QString title) {
    AddLiveGraphDialog algd(~valuesToShow,
                            solarAvailable,
                            hwType,
                            message,
                            this);
    algd.setWindowTitle(title);

    if (algd.exec() == QDialog::Accepted) {
        LiveValues columns = algd.selectedColumns();

        TEST_ADD_COL(LV_Temperature);
        TEST_ADD_COL(LV_ApparentTemperature);
        TEST_ADD_COL(LV_IndoorTemperature);
        TEST_ADD_COL(LV_WindChill);
        TEST_ADD_COL(LV_DewPoint);
        TEST_ADD_COL(LV_Humidity);
        TEST_ADD_COL(LV_IndoorHumidity);
        TEST_ADD_COL(LV_Pressure);
        TEST_ADD_COL(LV_BatteryVoltage);
        TEST_ADD_COL(LV_WindSpeed);
        TEST_ADD_COL(LV_WindDirection);
        TEST_ADD_COL(LV_RainRate);
        TEST_ADD_COL(LV_StormRain);
        TEST_ADD_COL(LV_UVIndex);
        TEST_ADD_COL(LV_SolarRadiation);

        ui->plot->replot();

        // If all possible values are now in the plot, disable the option to
        // add more (all the options will be greyed out if the user brings up
        // that dialog)
        ui->actionAdd_Graph->setEnabled(valuesToShow != ALL_LIVE_COLUMNS);
        ui->plot->setAddGraphsEnabled(valuesToShow != ALL_LIVE_COLUMNS);
    }
}

void LivePlotWindow::graphRemoving(QCPGraph *graph) {
    QVariant prop = graph->property(PROP_GRAPH_TYPE);
    if (prop.isNull() || !prop.isValid() || prop.type() != QVariant::Int) {
        return;
    }

    LiveValue graphType = (LiveValue)prop.toInt();

    valuesToShow &= ~graphType;

    graphs.remove(graphType);
    ui->plot->removeGraph(points[graphType]);
    points.remove(graphType);

    UnitConversions::unit_t axisType = units[graphType];
    if (axis[axisType]->graphs().count() == 1) {
        // The graph we're about to remove is the last graph using this axis
        // so the axis will end up being removed too. Remove the axis from our
        // list of axes so we don't accidentally use it again.
        axis.remove(axisType);
    }

    // Turn the legend off if we're removing the final graph.
    if (valuesToShow == LV_NoColumns && ui->plot->legend->visible()) {
        ui->plot->toggleLegend();
    }
}

void LivePlotWindow::selectionChanged() {
    bool graphsSelected = !ui->plot->selectedGraphs().isEmpty();
    ui->actionRemove_Graph->setEnabled(graphsSelected);
}

void LivePlotWindow::showOptions() {
    LiveChartOptionsDialog lcod(aggregate, aggregateSeconds, maxRainRate, stormRain,
                                hwType == HW_DAVIS, timespanMinutes, this);

    if (lcod.exec() == QDialog::Accepted) {
        if (aggregate != lcod.aggregate() || maxRainRate != lcod.maxRainRate() ||
                stormRain != lcod.stormRain() || aggregateSeconds != lcod.aggregatePeriod()) {
            // User changed settings.
            aggregate = lcod.aggregate();
            maxRainRate = lcod.maxRainRate();
            stormRain = lcod.stormRain();
            aggregateSeconds = lcod.aggregatePeriod();

            if (aggregate) {
                aggregator.reset(new AveragedLiveAggregator(aggregateSeconds, maxRainRate, stormRain, this));
            } else {
                aggregator.reset(new NonAggregatingLiveAggregator(stormRain, this));
            }

            connect(repeater.data(), SIGNAL(liveData(LiveDataSet)),
                    aggregator.data(), SLOT(incomingLiveData(LiveDataSet)));
            connect(aggregator.data(), SIGNAL(liveData(LiveDataSet)),
                    this, SLOT(liveData(LiveDataSet)));
        }
    }

    if (lcod.rangeMinutes() != timespanMinutes) {
        timespanMinutes = lcod.rangeMinutes();
    }

    Settings &settings = Settings::getInstance();
    settings.setLiveAggregate(aggregate);
    settings.setLiveMaxRainRate(maxRainRate);
    settings.setLiveStormRain(stormRain);
    settings.setLiveAggregateSeconds(aggregateSeconds);
    settings.setLiveTimespanMinutes(timespanMinutes);
}
