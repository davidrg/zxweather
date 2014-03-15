#include "chartwindow.h"
#include "ui_chartwindow.h"
#include "settings.h"
#include "addgraphdialog.h"

#include "datasource/webdatasource.h"
#include "datasource/databasedatasource.h"

#include <QFileDialog>
#include <QtDebug>
#include <QPen>
#include <QBrush>
#include <QMessageBox>
#include <QInputDialog>
#include <QMenu>
#include <QIcon>



ChartWindow::ChartWindow(SampleColumns columns,
                         QDateTime startTime,
                         QDateTime endTime,
                         QWidget *parent) :
    QWidget(parent),
    ui(new Ui::ChartWindow)
{
    ui->setupUi(this);

    gridVisible = true;

    basicInteractionManager.reset(
            new BasicQCPInteractionManager(ui->chart, this));

    plotter.reset(new WeatherPlotter(ui->chart, this));

    // These will be turned back on later if they are needed.
    ui->cbYLock->setVisible(false);
    ui->YLockDiv->setVisible(false);
    setYAxisLock();

    Settings& settings = Settings::getInstance();

    if (settings.sampleDataSourceType() == Settings::DS_TYPE_DATABASE)
        plotter->setDataSource(new DatabaseDataSource(this, this));
    else
        plotter->setDataSource(new WebDataSource(this, this));

    connect(ui->pbRefresh, SIGNAL(clicked()), this, SLOT(refresh()));
    connect(ui->saveButton, SIGNAL(clicked()), this, SLOT(save()));
    connect(ui->cbYLock, SIGNAL(toggled(bool)), this, SLOT(setYAxisLock()));

    // WeatherPlotter events
    connect(plotter.data(), SIGNAL(axisCountChanged(int)),
            this, SLOT(chartAxisCountChanged(int)));

    // Chart events


    connect(ui->chart, SIGNAL(titleDoubleClick(QMouseEvent*,QCPPlotTitle*)),
            this, SLOT(titleDoubleClick(QMouseEvent*, QCPPlotTitle*)));
    connect(ui->chart,
            SIGNAL(axisDoubleClick(QCPAxis*,
                                   QCPAxis::SelectablePart,
                                   QMouseEvent*)),
            this,
            SLOT(axisDoubleClick(QCPAxis*,
                                 QCPAxis::SelectablePart,
                                 QMouseEvent*)));
    connect(ui->chart, SIGNAL(customContextMenuRequested(QPoint)),
            this, SLOT(chartContextMenuRequested(QPoint)));



    setWindowTitle("Chart");

    ui->startTime->setDateTime(startTime);
    ui->endTime->setDateTime(endTime);
    plotter->drawChart(columns, startTime, endTime);
}

ChartWindow::~ChartWindow()
{
    delete ui;
}

void ChartWindow::refresh() {
    plotter->refresh(ui->startTime->dateTime(), ui->endTime->dateTime());
}

void ChartWindow::chartAxisCountChanged(int count) {
    if (count > 1) {
        // Now that we have multiple axes the Y Lock option becomes available.
        ui->YLockDiv->setVisible(true);
        ui->cbYLock->setVisible(true);
        setYAxisLock();
    } else {
        ui->YLockDiv->setVisible(false);
        ui->cbYLock->setVisible(false);
        setYAxisLock();
    }
}


void ChartWindow::setYAxisLock() {
    basicInteractionManager->setYAxisLockEnabled(ui->cbYLock->isEnabled()
                                   && ui->cbYLock->isChecked());
    ui->chart->deselectAll();
    ui->chart->replot();
}

void ChartWindow::axisDoubleClick(QCPAxis *axis, QCPAxis::SelectablePart part,
                                  QMouseEvent *event)
{
    // If the user double-clicked on the axis label then ask for new
    // label text.
    if (part == QCPAxis::spAxisLabel) {
        QString defaultLabel = plotter->defaultLabelForAxis(axis);
        bool ok;

        QString newLabel = QInputDialog::getText(
                    this,
                    defaultLabel + " Axis Label",
                    "New axis label:",
                    QLineEdit::Normal,
                    axis->label(),
                    &ok);
        if (ok) {
            axis->setLabel(newLabel);
            ui->chart->replot();
        }
    }
}

void ChartWindow::titleDoubleClick(QMouseEvent *event, QCPPlotTitle *title)
{
    Q_UNUSED(event)

    // Allow the graph title to be changed.
    bool ok;
    plotTitleValue = QInputDialog::getText(
                this,
                "Chart Title",
                "New chart title:",
                QLineEdit::Normal,
                title->text(),
                &ok);
    if (ok) {
        title->setText(plotTitleValue);
        ui->chart->replot();
    }
}

void ChartWindow::chartContextMenuRequested(QPoint point)
{

    if (ui->chart->legend->selectTest(point, false) >= 0
            && ui->chart->legend->visible()) {
        showLegendContextMenu(point);
        return;
    }

    QMenu* menu = new QMenu(this);
    menu->setAttribute(Qt::WA_DeleteOnClose);

    /******** Graph add/remove ********/

    // If a graph is currently selected let it be removed.
    if (!ui->chart->selectedGraphs().isEmpty()) {
        menu->addAction("Remove selected graph",
                        this, SLOT(removeSelectedGraph()));
    }

    QAction *action = menu->addAction(QIcon(":/icons/chart-add"), "Add Graph",
                                      this, SLOT(addGraph()));

    if (plotter->availableColumns() == 0) {
        // All graphs are already in the chart. No more to add.
        action->setEnabled(false);
    }

    /******** Plot feature visibility ********/
    menu->addSeparator();

    // Title visibility option.
    action = menu->addAction("Show Title",
                             this, SLOT(showTitleToggle()));
    action->setCheckable(true);
    action->setChecked(!plotTitle.isNull());


    // Legend visibility option.
    action = menu->addAction("Show Legend",
                             this, SLOT(showLegendToggle()));
    action->setCheckable(true);
    action->setChecked(ui->chart->legend->visible());

    // Grid visibility option
    action = menu->addAction("Show Grid",
                             this,
                             SLOT(showGridToggle()));
    action->setCheckable(true);
    action->setChecked(gridVisible);


    /******** Finished ********/
    menu->popup(ui->chart->mapToGlobal(point));
}

void ChartWindow::addTitle()
{
    bool ok;
    if (plotTitleValue.isNull()) {
        // Title has never been set. Ask for a value.
        plotTitleValue = QInputDialog::getText(
                    this,
                    "Chart Title",
                    "New chart title:",
                    QLineEdit::Normal,
                    "",
                    &ok);
    } else {
        // Just re-use the previous title.
        ok = true;
    }

    if (ok) {
        ui->chart->plotLayout()->insertRow(0);
        plotTitle = new QCPPlotTitle(ui->chart, plotTitleValue);
        ui->chart->plotLayout()->addElement(0, 0, plotTitle);
        ui->chart->replot();
    }
}

void ChartWindow::removeTitle()
{
    ui->chart->plotLayout()->remove(plotTitle);
    ui->chart->plotLayout()->simplify();
    ui->chart->replot();
}

void ChartWindow::showLegendToggle()
{
    ui->chart->legend->setVisible(!ui->chart->legend->visible());
    ui->chart->replot();
}

void ChartWindow::showTitleToggle()
{
    if (plotTitle.isNull())
        addTitle();
    else
        removeTitle();
}

void ChartWindow::showGridToggle() {

    gridVisible = !gridVisible;

    plotter->setAxisGridVisible(gridVisible);

    QList<QCPAxis*> axes = valueAxes();
    axes << ui->chart->xAxis;

    foreach(QCPAxis* axis, axes) {
        axis->grid()->setVisible(gridVisible);
    }
    ui->chart->replot();
}

void ChartWindow::moveLegend()
{
    if (QAction* menuAction = qobject_cast<QAction*>(sender())) {
        // We were called by a context menu. We should have the necessary
        // data.
        bool ok;
        int intData = menuAction->data().toInt(&ok);
        if (ok) {
            ui->chart->axisRect()->insetLayout()->setInsetAlignment(
                        0, (Qt::Alignment)intData);
            ui->chart->replot();
        }
    }
}

void ChartWindow::removeSelectedGraph()
{
    if (!ui->chart->selectedGraphs().isEmpty()) {
        QCPGraph* graph = ui->chart->selectedGraphs().first();

        // Turn off the column so it doesn't come back when the user
        // hits refresh.
        plotter->removeGraph((SampleColumn)graph->property(GRAPH_TYPE).toInt());
    }
}


QList<QCPAxis*> ChartWindow::valueAxes() {
    return ui->chart->axisRect()->axes(QCPAxis::atLeft | QCPAxis::atRight);
}

void ChartWindow::addGraph()
{
    AddGraphDialog adg(plotter->availableColumns(), this);
    if (adg.exec() == QDialog::Accepted)
        plotter->addGraphs(adg.selectedColumns());
}

void ChartWindow::showLegendContextMenu(QPoint point)
{
    QMenu *menu = new QMenu(this);
    menu->setAttribute(Qt::WA_DeleteOnClose);

    // Options to re-position the legend
    menu->addAction("Move to top left",
                    this,
                    SLOT(moveLegend()))->setData((int)(Qt::AlignTop
                                                       | Qt::AlignLeft));
    menu->addAction("Move to top center",
                    this,
                    SLOT(moveLegend()))->setData((int)(Qt::AlignTop
                                                       | Qt::AlignHCenter));
    menu->addAction("Move to top right",
                    this,
                    SLOT(moveLegend()))->setData((int)(Qt::AlignTop
                                                       | Qt::AlignRight));
    menu->addAction("Move to bottom right",
                    this,
                    SLOT(moveLegend()))->setData((int)(Qt::AlignBottom
                                                       | Qt::AlignRight));
    menu->addAction("Move to bottom center",
                    this,
                    SLOT(moveLegend()))->setData((int)(Qt::AlignBottom
                                                       | Qt::AlignHCenter));
    menu->addAction("Move to bottom left",
                    this,
                    SLOT(moveLegend()))->setData((int)(Qt::AlignBottom
                                                       | Qt::AlignLeft));

    // And an option to get rid of it entirely.
    menu->addSeparator();
    menu->addAction("Hide", this, SLOT(showLegendToggle()));

    menu->popup(ui->chart->mapToGlobal(point));
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
