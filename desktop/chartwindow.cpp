#include "chartwindow.h"
#include "ui_chartwindow.h"

#include "datasource/webdatasource.h"

#include <QFileDialog>
#include <QtDebug>

ChartWindow::ChartWindow(QWidget *parent) :
    QDialog(parent),
    ui(new Ui::ChartWindow)
{
    ui->setupUi(this);

    dataSource.reset(new WebDataSource("http://localhost:8080/", "rua", this, this));

    connect(ui->pbRefresh, SIGNAL(clicked()), this, SLOT(refresh()));
    connect(ui->saveButton, SIGNAL(clicked()), this, SLOT(save()));
    connect(dataSource.data(), SIGNAL(samplesReady(SampleSet)),
            this, SLOT(samplesReady(SampleSet)));

    // Configure chart

    ui->chart->setInteractions(QCustomPlot::iRangeZoom |
                               QCustomPlot::iSelectAxes |
                               QCustomPlot::iRangeDrag);
    ui->chart->setRangeDrag(Qt::Horizontal|Qt::Vertical);
    ui->chart->setRangeZoom(Qt::Horizontal|Qt::Vertical);
    ui->chart->setupFullAxesBox();
    ui->chart->xAxis->setLabel("Time");
    ui->chart->xAxis->setTickLabelType(QCPAxis::ltDateTime);

    // Chart events
    connect(ui->chart, SIGNAL(mousePress(QMouseEvent*)),
            this, SLOT(mousePress()));

    connect(ui->chart, SIGNAL(mouseWheel(QWheelEvent*)),
            this, SLOT(mouseWheel()));

    connect(ui->chart, SIGNAL(selectionChangedByUser()),
            this, SLOT(selectionChanged()));

    // Keep axis ranges locked
    connect(ui->chart->xAxis, SIGNAL(rangeChanged(QCPRange)),
            ui->chart->xAxis2, SLOT(setRange(QCPRange)));
    connect(ui->chart->yAxis, SIGNAL(rangeChanged(QCPRange)),
            ui->chart->yAxis2, SLOT(setRange(QCPRange)));

    // Setup chart type.
    ChartOptionsDialog options;
    int result = options.exec();
    if (result != QDialog::Accepted) {
        this->close();
    } else {
        columns = options.getColumns();
        ui->startTime->setDateTime(options.getStartTime());
        ui->endTime->setDateTime(options.getEndTime());
        chartType = options.getChartType();
        refresh();
    }
}

ChartWindow::~ChartWindow()
{
    delete ui;
}

void ChartWindow::refresh() {

    dataSource->fetchSamples(ui->startTime->dateTime(), ui->endTime->dateTime());
}

void ChartWindow::samplesReady(SampleSet samples) {

    qDebug() << "Samples: " << samples.sampleCount;

    ui->chart->clearGraphs();

    if (chartType == ChartOptionsDialog::Temperature)
        ui->chart->yAxis->setLabel("Temperature (\xB0""C)");
    else if (chartType == ChartOptionsDialog::Humidity)
        ui->chart->yAxis->setLabel("Humidity (%)");
    else
        ui->chart->yAxis->setLabel("Pressure (hPa)");

    foreach (int column, columns) {
        QCPGraph * graph = ui->chart->addGraph();
        if (column == COL_TEMPERATURE) {
            graph->setData(samples.timestamp, samples.temperature);
            graph->setName("Temperature");
        } else if (column == COL_TEMPERATURE_INDOORS) {
            graph->setData(samples.timestamp, samples.indoorTemperature);
            graph->setName("Indoor Temperature");
        } else if (column == COL_APPARENT_TEMPERATURE) {
            graph->setData(samples.timestamp, samples.apparentTemperature);
            graph->setName("Apparent Temperature");
        } else if (column == COL_WIND_CHILL) {
            graph->setData(samples.timestamp, samples.windChill);
            graph->setName("Wind Chill");
        } else if (column == COL_DEW_POINT) {
            graph->setData(samples.timestamp, samples.dewPoint);
            graph->setName("Dew Point");
        } else if (column == COL_HUMIDITY) {
            graph->setData(samples.timestamp, samples.humidity);
            graph->setName("Humidity");
        } else if (column == COL_HUMIDITY_INDOORS) {
            graph->setData(samples.timestamp, samples.indoorHumidity);
            graph->setName("Indoor Humidity");
        } else if (column == COL_PRESSURE) {
            graph->setData(samples.timestamp, samples.pressure);
            graph->setName("Pressure");
        }
    }

    if (columns.count() > 1)
        ui->chart->legend->setVisible(true);
    else
        ui->chart->legend->setVisible(false);

    ui->chart->rescaleAxes();
    ui->chart->replot();
}

void ChartWindow::mousePress() {
    // Only allow panning in the direction of the selected axis
    if (ui->chart->xAxis->selected().testFlag(QCPAxis::spAxis))
        ui->chart->setRangeDrag(ui->chart->xAxis->orientation());
    else if (ui->chart->yAxis->selected().testFlag(QCPAxis::spAxis))
        ui->chart->setRangeDrag(ui->chart->yAxis->orientation());
    else
        ui->chart->setRangeDrag(Qt::Horizontal|Qt::Vertical);
}

void ChartWindow::mouseWheel() {
    // Zoom on what ever axis is selected (if one is selected)
    if (ui->chart->xAxis->selected().testFlag(QCPAxis::spAxis))
        ui->chart->setRangeZoom(ui->chart->xAxis->orientation());
    else if (ui->chart->yAxis->selected().testFlag(QCPAxis::spAxis))
        ui->chart->setRangeZoom(ui->chart->yAxis->orientation());
    else
        ui->chart->setRangeZoom(Qt::Horizontal|Qt::Vertical);
}

void ChartWindow::selectionChanged() {
    // If either x axis or its tick labels is selected, select both axes
    if (ui->chart->xAxis->selected().testFlag(QCPAxis::spAxis) ||
            ui->chart->xAxis->selected().testFlag(QCPAxis::spTickLabels) ||
            ui->chart->xAxis2->selected().testFlag(QCPAxis::spAxis) ||
            ui->chart->xAxis2->selected().testFlag(QCPAxis::spTickLabels)) {

        ui->chart->xAxis->setSelected(QCPAxis::spAxis | QCPAxis::spTickLabels);
        ui->chart->xAxis2->setSelected(QCPAxis::spAxis | QCPAxis::spTickLabels);
    }

    // If either y axis or its tick labels is selected, select both axes
    if (ui->chart->yAxis->selected().testFlag(QCPAxis::spAxis) ||
            ui->chart->yAxis->selected().testFlag(QCPAxis::spTickLabels) ||
            ui->chart->yAxis2->selected().testFlag(QCPAxis::spAxis) ||
            ui->chart->yAxis2->selected().testFlag(QCPAxis::spTickLabels)) {

        ui->chart->yAxis->setSelected(QCPAxis::spAxis | QCPAxis::spTickLabels);
        ui->chart->yAxis2->setSelected(QCPAxis::spAxis | QCPAxis::spTickLabels);
    }
}

void ChartWindow::save() {

    QString pdfFilter = "Portable Document Format (*.pdf)";
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

    if (selectedFilter == pdfFilter)
        ui->chart->savePdf(fileName);
    else if (selectedFilter == pngFilter)
        ui->chart->savePng(fileName);
    else if (selectedFilter == jpgFilter)
        ui->chart->saveJpg(fileName);
    else if (selectedFilter == bmpFilter)
        ui->chart->saveBmp(fileName);
}

