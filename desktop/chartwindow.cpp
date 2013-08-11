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
                         enum ChartOptionsDialog::ChartType chartType,
                         QWidget *parent) :
    QWidget(parent),
    ui(new Ui::ChartWindow)
{
    ui->setupUi(this);
    this->columns = columns;
    this->chartType = chartType;
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

    // Configure chart

    ui->chart->setInteractions(QCP::iRangeZoom |
                               QCP::iSelectAxes |
                               QCP::iRangeDrag);
    ui->chart->axisRect()->setRangeDrag(Qt::Horizontal|Qt::Vertical);
    ui->chart->axisRect()->setRangeZoom(Qt::Horizontal|Qt::Vertical);
    ui->chart->axisRect()->setupFullAxesBox();
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

    if (chartType == ChartOptionsDialog::Temperature)
        setWindowTitle("Temperature Chart");
    else if (chartType == ChartOptionsDialog::Humidity)
        setWindowTitle("Humidity Chart");
    else if (chartType == ChartOptionsDialog::Pressure)
        setWindowTitle("Pressure Chart");
    else if (chartType == ChartOptionsDialog::Rainfall)
        setWindowTitle("Rainfall Chart");
    refresh();
}

ChartWindow::~ChartWindow()
{
    delete ui;
}

void ChartWindow::refresh() {

    dataSource->fetchSamples(ui->startTime->dateTime(), ui->endTime->dateTime());
}

void ChartWindow::samplesReady(SampleSet samples) {

    ChartColours colours = Settings::getInstance().getChartColours();

    qDebug() << "Samples: " << samples.sampleCount;

    ui->chart->clearGraphs();
    ui->chart->clearPlottables();

    if (chartType == ChartOptionsDialog::Temperature)
        ui->chart->yAxis->setLabel("Temperature (\xB0""C)");
    else if (chartType == ChartOptionsDialog::Humidity)
        ui->chart->yAxis->setLabel("Humidity (%)");
    else
        ui->chart->yAxis->setLabel("Pressure (hPa)");

    foreach (int column, columns) {
        QCPGraph * graph = NULL;
//        if (column != COL_RAINFALL)
            graph = ui->chart->addGraph();

        if (column == COL_TEMPERATURE) {
            graph->setData(samples.timestamp, samples.temperature);
            graph->setName("Temperature");
            graph->setPen(QPen(colours.temperature));
        } else if (column == COL_TEMPERATURE_INDOORS) {
            graph->setData(samples.timestamp, samples.indoorTemperature);
            graph->setName("Indoor Temperature");
            graph->setPen(QPen(colours.indoorTemperature));
        } else if (column == COL_APPARENT_TEMPERATURE) {
            graph->setData(samples.timestamp, samples.apparentTemperature);
            graph->setName("Apparent Temperature");
            graph->setPen(QPen(colours.apparentTemperature));
        } else if (column == COL_WIND_CHILL) {
            graph->setData(samples.timestamp, samples.windChill);
            graph->setName("Wind Chill");
            graph->setPen(QPen(colours.windChill));
        } else if (column == COL_DEW_POINT) {
            graph->setData(samples.timestamp, samples.dewPoint);
            graph->setName("Dew Point");
            graph->setPen(QPen(colours.dewPoint));
        } else if (column == COL_HUMIDITY) {
            graph->setData(samples.timestamp, samples.humidity);
            graph->setName("Humidity");
            graph->setPen(QPen(colours.humidity));
        } else if (column == COL_HUMIDITY_INDOORS) {
            graph->setData(samples.timestamp, samples.indoorHumidity);
            graph->setName("Indoor Humidity");
            graph->setPen(QPen(colours.indoorHumidity));
        } else if (column == COL_PRESSURE) {
            graph->setData(samples.timestamp, samples.pressure);
            graph->setName("Pressure");
            graph->setPen(QPen(colours.pressure));
        } else if (column == COL_RAINFALL) {
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
        }
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

void ChartWindow::mousePress() {
    // Only allow panning in the direction of the selected axis
    if (ui->chart->xAxis->selectedParts().testFlag(QCPAxis::spAxis))
        ui->chart->axisRect()->setRangeDrag(ui->chart->xAxis->orientation());
    else if (ui->chart->yAxis->selectedParts().testFlag(QCPAxis::spAxis))
        ui->chart->axisRect()->setRangeDrag(ui->chart->yAxis->orientation());
    else
        ui->chart->axisRect()->setRangeDrag(Qt::Horizontal|Qt::Vertical);
}

void ChartWindow::mouseWheel() {
    // Zoom on what ever axis is selected (if one is selected)
    if (ui->chart->xAxis->selectedParts().testFlag(QCPAxis::spAxis))
        ui->chart->axisRect()->setRangeZoom(ui->chart->xAxis->orientation());
    else if (ui->chart->yAxis->selectedParts().testFlag(QCPAxis::spAxis))
        ui->chart->axisRect()->setRangeZoom(ui->chart->yAxis->orientation());
    else
        ui->chart->axisRect()->setRangeZoom(Qt::Horizontal|Qt::Vertical);
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

    // If either y axis or its tick labels is selected, select both axes
    if (ui->chart->yAxis->selectedParts().testFlag(QCPAxis::spAxis) ||
            ui->chart->yAxis->selectedParts().testFlag(QCPAxis::spTickLabels) ||
            ui->chart->yAxis2->selectedParts().testFlag(QCPAxis::spAxis) ||
            ui->chart->yAxis2->selectedParts().testFlag(QCPAxis::spTickLabels)) {

        ui->chart->yAxis->setSelectedParts(QCPAxis::spAxis | QCPAxis::spTickLabels);
        ui->chart->yAxis2->setSelectedParts(QCPAxis::spAxis | QCPAxis::spTickLabels);
    }
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

