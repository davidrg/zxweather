#include "chartwindow.h"
#include "ui_chartwindow.h"
#include "settings.h"

#include "datasource/webdatasource.h"
#include "datasource/databasedatasource.h"

#include <QFileDialog>
#include <QtDebug>
#include <QPen>
#include <QBrush>
#include <QMessageBox>

ChartWindow::ChartWindow(QList<int> columns,
                         QDateTime startTime,
                         QDateTime endTime,
                         QWidget *parent) :
    QWidget(parent),
    ui(new Ui::ChartWindow)
{
    ui->setupUi(this);
    populateAxisLabels();

    // These will be turned back on later if they are needed.
    ui->cbYLock->setVisible(false);
    ui->YLockDiv->setVisible(false);

    this->columns = columns;
    ui->startTime->setDateTime(startTime);
    ui->endTime->setDateTime(endTime);

    Settings& settings = Settings::getInstance();

    if (settings.sampleDataSourceType() == Settings::DS_TYPE_DATABASE)
        dataSource.reset(new DatabaseDataSource(this, this));
    else
        dataSource.reset(new WebDataSource(this, this));

    connect(ui->pbRefresh, SIGNAL(clicked()), this, SLOT(refresh()));
    connect(ui->saveButton, SIGNAL(clicked()), this, SLOT(save()));
    connect(dataSource.data(), SIGNAL(samplesReady(SampleSet)),
            this, SLOT(samplesReady(SampleSet)));
    connect(dataSource.data(), SIGNAL(sampleRetrievalError(QString)),
            this, SLOT(samplesError(QString)));
    connect(ui->cbYLock, SIGNAL(toggled(bool)), this, SLOT(axisLockToggled()));

    // Configure chart
    ui->chart->setInteractions(QCP::iRangeZoom |
                               QCP::iSelectAxes |
                               QCP::iRangeDrag);
    ui->chart->axisRect()->setRangeDrag(Qt::Horizontal|Qt::Vertical);
    ui->chart->axisRect()->setRangeZoom(Qt::Horizontal|Qt::Vertical);
    //ui->chart->axisRect()->setupFullAxesBox();
    ui->chart->xAxis->setLabel("Time");
    ui->chart->xAxis->setTickLabelType(QCPAxis::ltDateTime);

    // Chart events
    connect(ui->chart, SIGNAL(mousePress(QMouseEvent*)),
            this, SLOT(mousePress(QMouseEvent*)));
    connect(ui->chart, SIGNAL(mouseMove(QMouseEvent*)),
            this, SLOT(mouseMove(QMouseEvent*)));
    connect(ui->chart, SIGNAL(mouseRelease(QMouseEvent*)),
            this, SLOT(mouseRelease()));
    connect(ui->chart, SIGNAL(mouseWheel(QWheelEvent*)),
            this, SLOT(mouseWheel(QWheelEvent*)));

    connect(ui->chart, SIGNAL(selectionChangedByUser()),
            this, SLOT(selectionChanged()));

    // Keep axis ranges locked
    connect(ui->chart->xAxis, SIGNAL(rangeChanged(QCPRange)),
            ui->chart->xAxis2, SLOT(setRange(QCPRange)));

    setWindowTitle("Chart");

    refresh();
}

ChartWindow::~ChartWindow()
{
    delete ui;
}

void ChartWindow::populateAxisLabels() {
    axisLabels.insert(AT_HUMIDITY, "Humidity (%)");
    axisLabels.insert(AT_PRESSURE, "Pressure (hPa)");
    axisLabels.insert(AT_RAINFALL, "Rainfall (mm)");
    axisLabels.insert(AT_TEMPERATURE, "Temperature (\xB0""C)");
    axisLabels.insert(AT_WIND_SPEED, "Wind speed (m/s)");
    axisLabels.insert(AT_WIND_DIRECTION, "Wind direction (degrees)");
}

void ChartWindow::refresh() {

    dataSource->fetchSamples(ui->startTime->dateTime(), ui->endTime->dateTime());
}


QPointer<QCPAxis> ChartWindow::createAxis(AxisType type) {
    QCPAxis* axis = NULL;
    if (configuredAxes.isEmpty()) {
        axis = ui->chart->yAxis;
    } else if (configuredAxes.count() == 1) {
        axis = ui->chart->yAxis2;
        axis->setVisible(true);
        axis->setTickLabels(true);
    } else {
        // Every second axis can go on the right.
        if (configuredAxes.count() % 2 == 0)
            axis = ui->chart->axisRect()->addAxis(QCPAxis::atLeft);
        else
            axis = ui->chart->axisRect()->addAxis(QCPAxis::atRight);
    }
    configuredAxes.insert(type, axis);
    mDragStartVertRange.insert(type, QCPRange());
    axis->setLabel(axisLabels[type]);

    if (configuredAxes.count() > 1) {
        // Now that we have multiple axes the Y Lock option becomes available.
        ui->YLockDiv->setVisible(true);
        ui->cbYLock->setVisible(true);
    }


    return axis;
}

QPointer<QCPAxis> ChartWindow::getValueAxis(AxisType axisType) {
    QPointer<QCPAxis> axis = NULL;
    if (!configuredAxes.contains(axisType))
        // Axis of specified type doesn't exist. Create it.
        axis = createAxis(axisType);
    else
        // Axis already exists
        axis = configuredAxes[axisType];

    return axis;
}

void ChartWindow::samplesReady(SampleSet samples) {

    ChartColours colours = Settings::getInstance().getChartColours();

    qDebug() << "Samples: " << samples.sampleCount;

    ui->chart->clearGraphs();
    ui->chart->clearPlottables();

    /* Required axes:
     *  Temperature (degrees C)
     *  Speed (m/s)
     *  Pressure (hPa)
     *  Humidity (%)
     *  Rainfall (mm)
     */

    foreach (int column, columns) {
        QCPGraph * graph = NULL;
//        if (column != COL_RAINFALL)
            graph = ui->chart->addGraph();

        if (column == COL_TEMPERATURE) {
            graph->setValueAxis(getValueAxis(AT_TEMPERATURE));
            graph->setData(samples.timestamp, samples.temperature);
            graph->setName("Temperature");
            graph->setPen(QPen(colours.temperature));
        } else if (column == COL_TEMPERATURE_INDOORS) {
            graph->setValueAxis(getValueAxis(AT_TEMPERATURE));
            graph->setData(samples.timestamp, samples.indoorTemperature);
            graph->setName("Indoor Temperature");
            graph->setPen(QPen(colours.indoorTemperature));
        } else if (column == COL_APPARENT_TEMPERATURE) {
            graph->setValueAxis(getValueAxis(AT_TEMPERATURE));
            graph->setData(samples.timestamp, samples.apparentTemperature);
            graph->setName("Apparent Temperature");
            graph->setPen(QPen(colours.apparentTemperature));
        } else if (column == COL_WIND_CHILL) {
            graph->setValueAxis(getValueAxis(AT_TEMPERATURE));
            graph->setData(samples.timestamp, samples.windChill);
            graph->setName("Wind Chill");
            graph->setPen(QPen(colours.windChill));
        } else if (column == COL_DEW_POINT) {
            graph->setValueAxis(getValueAxis(AT_TEMPERATURE));
            graph->setData(samples.timestamp, samples.dewPoint);
            graph->setName("Dew Point");
            graph->setPen(QPen(colours.dewPoint));
        } else if (column == COL_HUMIDITY) {
            graph->setValueAxis(getValueAxis(AT_HUMIDITY));
            graph->setData(samples.timestamp, samples.humidity);
            graph->setName("Humidity");
            graph->setPen(QPen(colours.humidity));
        } else if (column == COL_HUMIDITY_INDOORS) {
            graph->setValueAxis(getValueAxis(AT_HUMIDITY));
            graph->setData(samples.timestamp, samples.indoorHumidity);
            graph->setName("Indoor Humidity");
            graph->setPen(QPen(colours.indoorHumidity));
        } else if (column == COL_PRESSURE) {
            graph->setValueAxis(getValueAxis(AT_PRESSURE));
            graph->setData(samples.timestamp, samples.pressure);
            graph->setName("Pressure");
            graph->setPen(QPen(colours.pressure));
        } else if (column == COL_RAINFALL) {
            graph->setValueAxis(getValueAxis(AT_RAINFALL));
            // How do you plot rainfall data so it doesn't look stupid?
            // I don't know. Needs to be lower resolution I guess.
            graph->setData(samples.timestamp, samples.rainfall);
            graph->setName("Rainfall");
            graph->setPen(QPen(colours.rainfall));
            //graph->setLineStyle(QCPGraph::lsNone);
            //graph->setScatterStyle(QCP::ssCross);
//            QCPBars *bars = new QCPBars(ui->chart->xAxis, ui->chart->yAxis);
//            ui->chart->addPlottable(bars);
//            bars->setData(samples.timestamp, samples.rainfall);
//            bars->setName("Rainfall");
//            bars->setPen(QPen(Qt::darkBlue));
//            bars->setBrush(QBrush(Qt::green));
//            bars->setWidth(1000);
            // set pen
        } else if (column == COL_AVG_WINDSPEED) {
            graph->setValueAxis(getValueAxis(AT_WIND_SPEED));
            graph->setData(samples.timestamp, samples.averageWindSpeed);
            graph->setName("Average Wind Speed");
            graph->setPen(QPen(colours.averageWindSpeed));
        } else if (column == COL_GUST_WINDSPEED) {
            graph->setValueAxis(getValueAxis(AT_WIND_SPEED));
            graph->setData(samples.timestamp, samples.gustWindSpeed);
            graph->setName("Gust Wind Speed");
            graph->setPen(QPen(colours.gustWindSpeed));
        } else if (column == COL_WIND_DIRECTION) {
            graph->setValueAxis(getValueAxis(AT_WIND_DIRECTION));
            QList<uint> keys = samples.windDirection.keys();
            qSort(keys.begin(), keys.end());
            QVector<double> timestamps;
            QVector<double> values;
            foreach(uint key, keys) {
                timestamps.append(key);
                values.append(samples.windDirection[key]);
            }
            graph->setData(timestamps,values);
            graph->setName("Wind Direction");
            graph->setPen(QPen(colours.windDirection));
        }

        graph->rescaleAxes();
    }

    if (columns.count() > 1)
        ui->chart->legend->setVisible(true);
    else
        ui->chart->legend->setVisible(false);

    ui->chart->rescaleAxes();
    ui->chart->replot();
}

void ChartWindow::samplesError(QString message)
{
    QMessageBox::critical(this, "Error", message);
}

bool ChartWindow::isAnyYAxisSelected() {
    bool yAxisPartSelected = false;
    foreach(QPointer<QCPAxis> axis, configuredAxes) {
        if (axis->selectedParts().testFlag(QCPAxis::spAxis) ||
                axis->selectedParts().testFlag(QCPAxis::spTickLabels))
            yAxisPartSelected = true;
    }

    return yAxisPartSelected;
}

QPointer<QCPAxis> ChartWindow::valueAxisWithSelectedParts() {
    foreach(QPointer<QCPAxis> axis, configuredAxes) {
        if (axis->selectedParts().testFlag(QCPAxis::spAxis) ||
                axis->selectedParts().testFlag(QCPAxis::spTickLabels))
            return axis;
    }
    return 0;
}

void ChartWindow::mousePress(QMouseEvent *event) {
    // Only allow panning in the direction of the selected axis
    if (ui->chart->xAxis->selectedParts().testFlag(QCPAxis::spAxis))
        ui->chart->axisRect()->setRangeDrag(ui->chart->xAxis->orientation());
    else if (isAnyYAxisSelected() && !isYAxisLockOn()) {
        QPointer<QCPAxis> axis = valueAxisWithSelectedParts();
        ui->chart->axisRect()->setRangeDrag(axis->orientation());
        ui->chart->axisRect()->setRangeDragAxes(ui->chart->xAxis, axis);
    } else {
        /* No specific axis selected. Pan all the axes!
         *
         * Problem: QCustomPlot can't pan more than one set of axes. Damn.
         *
         * Solution: Let QCustomPlot pan X1,Y1 and we'll have to manually
         *           pan all the remaining Y axes. :|
         *
         */
        ui->chart->axisRect()->setRangeDragAxes(ui->chart->xAxis,
                                                ui->chart->yAxis);
        if (isAnyYAxisSelected()) {
            // A Y axis is selected. If we got in here then Y Axis Lock must
            // be on. We'll only pan vertically.
            ui->chart->axisRect()->setRangeDrag(Qt::Vertical);
        } else {
            ui->chart->axisRect()->setRangeDrag(Qt::Horizontal|Qt::Vertical);
        }

        mDragStart = event->pos();
        if (event->buttons() & Qt::LeftButton) {
            mDragging = true;
            // We probably don't need to mess with AA settings -
            // QCPAxisRect::mousePressEvent(QMouseEvent*) should fire next
            // and take care of that for us.

            // We have to store multiple vertical start ranges (one for each
            // Y axis).
            foreach (AxisType axisType, configuredAxes.keys()) {
                QPointer<QCPAxis> axis = configuredAxes[axisType];
                QCPRange range = axis->range();
                mDragStartVertRange[axisType] = range;
            }
        }
    }
}

void ChartWindow::mouseMove(QMouseEvent *event){
    if (mDragging) {
        foreach(AxisType axisType, configuredAxes.keys()) {
            QPointer<QCPAxis> axis = configuredAxes[axisType];

            // QCustomPlot will handle RangeDrag for y1. We only need to do
            // all the others.
            if (axis == ui->chart->yAxis)
                continue;

            // This logic is exactly the same as what can be found in
            // QCPAxisRect::mouseMoveEvent(QMouseEvent*) from QCustomPlot 1.0.0

            if (axis->scaleType() == QCPAxis::stLinear) {

              double diff = axis->pixelToCoord(mDragStart.y())
                      - axis->pixelToCoord(event->pos().y());

              axis->setRange(
                          mDragStartVertRange[axisType].lower + diff,
                          mDragStartVertRange[axisType].upper + diff);

            } else if (axis->scaleType() == QCPAxis::stLogarithmic) {
              double diff = axis->pixelToCoord(mDragStart.y())
                      / axis->pixelToCoord(event->pos().y());
              axis->setRange(
                          mDragStartVertRange[axisType].lower * diff,
                          mDragStartVertRange[axisType].upper * diff);
            }
        }
        // We shouldn't need to do a replot -
        // QCPAxisRect::mouseMoveEvent(QMouseEvent*) should fire next and
        // handle it for us.
    }
}

void ChartWindow::mouseRelease() {
    mDragging = false;
    // QCPAxisRect::mouseReleaseEvent(QMouseEvent *) should fire next and
    // deal with the AA stuff so we shouldn't need to here.
}

void ChartWindow::mouseWheel(QWheelEvent* event) {
    // Zoom on what ever axis is selected (if one is selected)
    if (ui->chart->xAxis->selectedParts().testFlag(QCPAxis::spAxis))
        ui->chart->axisRect()->setRangeZoom(ui->chart->xAxis->orientation());
    else if (isAnyYAxisSelected() && !isYAxisLockOn()) {
        // A Y axis is selected and axis lock is not on. So we'll just scale
        // that one axis.
        QPointer<QCPAxis> axis = valueAxisWithSelectedParts();
        ui->chart->axisRect()->setRangeZoom(axis->orientation());
        ui->chart->axisRect()->setRangeZoomAxes(ui->chart->xAxis, axis);
    } else {
        /* No specific axis selected. Zoom all the axes!
         *
         * Problem: QCustomPlot can't zoom multiple axes. Damn.
         *
         * Solution: Let it zoom x1 and y1 and we'll handle zooming all the
         *           other y axes manually :|
         */
        ui->chart->axisRect()->setRangeZoomAxes(ui->chart->xAxis,
                                                ui->chart->yAxis);
        if (isAnyYAxisSelected()) {
            // A Y axis is selected. If we got in here then Y axis lock must
            // be on. We'll only scale vertically.
            ui->chart->axisRect()->setRangeZoom(Qt::Vertical);
        } else {
            ui->chart->axisRect()->setRangeZoom(Qt::Horizontal|Qt::Vertical);
        }

        // This is the same logic as used by QCustomPlot v1.0.0 in
        // QCPAxisRect::wheelEvent(QWheelEvent*)

        // a single step delta is +/-120 usually
        double wheelSteps = event->delta()/120.0;
        double verticalRangeZoomFactor =
                ui->chart->axisRect()->rangeZoomFactor(Qt::Vertical);

        foreach (QPointer<QCPAxis> axis, configuredAxes) {
            double factor = pow(verticalRangeZoomFactor, wheelSteps);
            // We don't want to scale y1 - QCustomPlot will handle that.
            if (axis != ui->chart->yAxis) {
                axis->scaleRange(factor,
                                 axis->pixelToCoord(event->pos().y()));
            }
        }
    }
}

void ChartWindow::selectionChanged() {
    // If either x axis or its tick labels is selected, select both axes
    if (ui->chart->xAxis->selectedParts().testFlag(QCPAxis::spAxis) ||
            ui->chart->xAxis->selectedParts().testFlag(QCPAxis::spTickLabels) ||
            ui->chart->xAxis2->selectedParts().testFlag(QCPAxis::spAxis) ||
            ui->chart->xAxis2->selectedParts().testFlag(QCPAxis::spTickLabels)) {

        ui->chart->xAxis->setSelectedParts(QCPAxis::spAxis | QCPAxis::spTickLabels);
        ui->chart->xAxis2->setSelectedParts(QCPAxis::spAxis | QCPAxis::spTickLabels);
    }

    // If either y axis or its tick labels are selected, select both axes
    if (isAnyYAxisSelected()) {
        // If one part (tick labels or the actual axis) is selected, ensure
        // both are.

        if (isYAxisLockOn()) {
            // ALL axes should be selected
            foreach (AxisType axisType, configuredAxes.keys()) {
                QPointer<QCPAxis> axis = configuredAxes[axisType];
                axis->setSelectedParts(QCPAxis::spAxis |
                                       QCPAxis::spTickLabels);
            }
        } else {
            // Just ensure the axis is fully selected.
            QPointer<QCPAxis> axis = valueAxisWithSelectedParts();
            axis->setSelectedParts(QCPAxis::spAxis | QCPAxis::spTickLabels);
        }
    }
}

void ChartWindow::axisLockToggled() {
    ui->chart->deselectAll();
    ui->chart->replot();
}

bool ChartWindow::isYAxisLockOn() {
    return ui->cbYLock->isVisible() && ui->cbYLock->isChecked();
}

void ChartWindow::save() {

    QString pdfFilter = "Adobe Portable Document Format (*.pdf)";
    QString pngFilter = "Portable Network Graphics (*.png)";
    QString jpgFilter = "JPEG (*.jpg)";
    QString bmpFilter = "Windows Bitmap (*.bmp)";

    QString filter = pngFilter + ";;" + pdfFilter + ";;" +
            jpgFilter + ";;" + bmpFilter;

    QString selectedFilter;

    QString fileName = QFileDialog::getSaveFileName(
                this, "Save As" ,"", filter, &selectedFilter);


    qDebug() << selectedFilter;
    qDebug() << fileName;

    // To prevent selected stuff appearing in the output
    ui->chart->deselectAll();

    // TODO: Provide a UI to ask for width, height, cosmetic pen (PS), etc.
    if (selectedFilter == pdfFilter)
        ui->chart->savePdf(fileName);
    else if (selectedFilter == pngFilter)
        ui->chart->savePng(fileName);
    else if (selectedFilter == jpgFilter)
        ui->chart->saveJpg(fileName);
    else if (selectedFilter == bmpFilter)
        ui->chart->saveBmp(fileName);
}

