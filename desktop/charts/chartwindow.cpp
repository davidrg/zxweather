#include "chartwindow.h"
#include "ui_chartwindow.h"
#include "settings.h"
#include "addgraphdialog.h"
#include "customisechartdialog.h"
#include "graphstyledialog.h"
#include "datasettimespandialog.h"

#include "datasource/webdatasource.h"
#include "datasource/databasedatasource.h"
#include "datasource/dialogprogresslistener.h"

#include <QFileDialog>
#include <QtDebug>
#include <QPen>
#include <QBrush>
#include <QMessageBox>
#include <QInputDialog>
#include <QMenu>
#include <QIcon>

// single-data-set functionality needs to know which the first (and only) data
// set is.
#define FIRST_DATA_SET 0

// This enables multi-data-set support in the UI,
#define MULTI_DATA_SET

#ifdef QT_DEBUG
// This isn't finished yet. So disabled in release builds.
#define CUSTOMISE_CHART
#endif

#ifdef MULTI_DATA_SET
// Uncomment to hide the controls at the top of the window
//#define NO_FORM_CONTROLS
#endif


ChartWindow::ChartWindow(QList<DataSet> dataSets, bool solarAvailable,
                         QWidget *parent) :
    QWidget(parent),
    ui(new Ui::ChartWindow)
{
    ui->setupUi(this);       

    solarDataAvailable = solarAvailable;
    gridVisible = true;
    plotTitleEnabled = false;

    basicInteractionManager.reset(
            new BasicQCPInteractionManager(ui->chart, this));

    plotter.reset(new WeatherPlotter(ui->chart, this));

    // These will be turned back on later if they are needed.
    ui->cbYLock->setVisible(false);
    ui->YLockDiv->setVisible(false);
    setYAxisLock();
    setXAxisLock();

    Settings& settings = Settings::getInstance();

    ChartColours colours = settings.getChartColours();
    plotTitleColour = colours.title;
    plotBackgroundBrush = QBrush(colours.background);

    ui->chart->axisRect()->setBackground(plotBackgroundBrush);

    AbstractDataSource *ds;
    if (settings.sampleDataSourceType() == Settings::DS_TYPE_DATABASE)
        ds = new DatabaseDataSource(new DialogProgressListener(this), this);
    else
        ds = new WebDataSource(new DialogProgressListener(this), this);

    hw_type = ds->getHardwareType();
    plotter->setDataSource(ds);

    connect(ui->pbRefresh, SIGNAL(clicked()), this, SLOT(refresh()));
    connect(ui->saveButton, SIGNAL(clicked()), this, SLOT(save()));
    connect(ui->cbYLock, SIGNAL(toggled(bool)), this, SLOT(setYAxisLock()));
    connect(ui->cbXLock, SIGNAL(toggled(bool)), this, SLOT(setXAxisLock()));

    // WeatherPlotter events
    connect(plotter.data(), SIGNAL(axisCountChanged(int)),
            this, SLOT(chartAxisCountChanged(int)));
    connect(plotter.data(), SIGNAL(dataSetRemoved(dataset_id_t)),
            this, SLOT(dataSetRemoved(dataset_id_t)));

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
    connect(ui->chart, SIGNAL(plottableDoubleClick(QCPAbstractPlottable*,QMouseEvent*)),
            this, SLOT(plottableDoubleClick(QCPAbstractPlottable*,QMouseEvent*)));
    connect(ui->chart,
            SIGNAL(legendDoubleClick(QCPLegend*,QCPAbstractLegendItem*,
                                     QMouseEvent*)),
            this,
            SLOT(legendDoubleClick(QCPLegend*,QCPAbstractLegendItem*,
                                   QMouseEvent*)));


#ifdef NO_FORM_CONTROLS
    ui->startTime->setVisible(false);
    ui->lblStartTime->setVisible(false);
    ui->endTime->setVisible(false);
    ui->lblEndTime->setVisible(false);
    ui->pbRefresh->setVisible(false);
    ui->divRefresh->setVisible(false);
    ui->cbXLock->setVisible(false);
    ui->cbYLock->setVisible(false);
    ui->YLockDiv->setVisible(false);
    ui->saveButton->setVisible(false);
#endif

    setWindowTitle("Chart");

    this->dataSets = dataSets;

    reloadDataSets(true);
}

ChartWindow::~ChartWindow()
{
    delete ui;
}

void ChartWindow::reloadDataSets(bool rebuildChart) {
#ifndef NO_FORM_CONTROLS
    if (dataSets.count() > 1) {
        // If we have multiple datasets then we can't use the simple
        // start/end time boxes.
        ui->startTime->setVisible(false);
        ui->lblStartTime->setVisible(false);
        ui->endTime->setVisible(false);
        ui->lblEndTime->setVisible(false);
        ui->pbRefresh->setVisible(false);
        ui->divRefresh->setVisible(false);
    } else {
        ui->startTime->setDateTime(dataSets.first().startTime);
        ui->endTime->setDateTime(dataSets.first().endTime);
        ui->startTime->setVisible(true);
        ui->endTime->setVisible(true);
        ui->lblEndTime->setVisible(true);
        ui->pbRefresh->setVisible(true);
        ui->divRefresh->setVisible(true);
    }
#endif

    if (rebuildChart) {
        // Reset the IDs on all incoming datasets to ensure they're unique.
        for (int i = 0; i < dataSets.count(); i++) {
            dataSets[i].id = i;
        }

        plotter->drawChart(dataSets);
    }
}

void ChartWindow::dataSetRemoved(dataset_id_t dataSetId) {
    for(int i = 0; i < dataSets.count(); i++) {
        if (dataSets[i].id == dataSetId) {
            dataSets.removeAt(i);
            return;
        }
    }
    qWarning() << "Could not find removed data set" << dataSetId;
}

void ChartWindow::refresh() {
    // Update the first dataset only
    plotter->changeDataSetTimespan(FIRST_DATA_SET,
                                   ui->startTime->dateTime(),
                                   ui->endTime->dateTime());
}

void ChartWindow::chartAxisCountChanged(int count) {
#ifndef NO_FORM_CONTROLS
    if (count > 1) {
        // Now that we have multiple axes the Y Lock option becomes available.
        ui->YLockDiv->setVisible(true);
        ui->cbYLock->setVisible(true);
    } else {
        ui->YLockDiv->setVisible(false);
        ui->cbYLock->setVisible(false);
    }
#endif
    setYAxisLock();
}

void ChartWindow::setYAxisLock() {
    basicInteractionManager->setYAxisLockEnabled(ui->cbYLock->isEnabled()
                                   && ui->cbYLock->isChecked());
    ui->chart->deselectAll();
    ui->chart->replot();
}

void ChartWindow::toggleYAxisLock() {
    ui->cbYLock->setChecked(!ui->cbYLock->isChecked());
    setYAxisLock();
}

void ChartWindow::setXAxisLock() {
    basicInteractionManager->setXAxisLockEnabled(ui->cbXLock->isEnabled()
                                                 && ui->cbXLock->isChecked());
    ui->chart->deselectAll();
    ui->chart->replot();
}

void ChartWindow::toggleXAxisLock() {
    ui->cbXLock->setChecked(!ui->cbXLock->isChecked());
    setXAxisLock();
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
    // Check to see if the legend was right-clicked on
    if (ui->chart->legend->selectTest(point, false) >= 0
            && ui->chart->legend->visible()) {
        showLegendContextMenu(point);
        return;
    }

    // Check if an X axis was right-clicked on
    QList<QCPAxis*> keyAxes = ui->chart->axisRect()->axes(QCPAxis::atTop |
                                                          QCPAxis::atBottom);
    foreach (QCPAxis* keyAxis, keyAxes) {
        if (keyAxis->selectTest(point, false) >= 0) {
            showKeyAxisContextMenu(point, keyAxis);
            return;
        }
    }

    // Commented out: Intended functions were rename and move to opposite. Turns
    // out only one of those is possbile and the other probably not so useful.
//    // Check if a Y axis was right-clicked on
//    QList<QCPAxis*> valueAxes = ui->chart->axisRect()->axes(QCPAxis::atLeft |
//                                                          QCPAxis::atRight);
//    foreach (QCPAxis* valueAxis, valueAxes) {
//        if (valueAxis->selectTest(point, false) >= 0) {
//            showValueAxisContextMenu(point, valueAxis);
//            return;
//        }
//    }

    showChartContextMenu(point);
}


void ChartWindow::showChartContextMenu(QPoint point) {
    QMenu* menu = new QMenu(this);
    menu->setAttribute(Qt::WA_DeleteOnClose);

    QAction *action;

#ifdef MULTI_DATA_SET
    menu->addAction("Add Data Set...",
                    this, SLOT(addDataSet()));

    // If a graph is currently selected show some extra options.
    if (!ui->chart->selectedGraphs().isEmpty()) {
        QMenu* graphMenu = menu->addMenu("&Selected Graph");
        graphMenu->addAction("&Rename...", this, SLOT(renameSelectedGraph()));
        graphMenu->addAction("&Change Style...", this,
                             SLOT(changeSelectedGraphStyle()));
        graphMenu->addSeparator();
        graphMenu->addAction("R&emove", this, SLOT(removeSelectedGraph()));
    }

#else
    /******** Graph remove ********/

    // If a graph is currently selected let it be removed.
    if (!ui->chart->selectedGraphs().isEmpty()) {
        menu->addAction("Remove selected graph",
                        this, SLOT(removeSelectedGraph()));
    }
#endif

    menu->addSeparator();
    menu->addAction(QIcon(":/icons/save"), "&Save...", this, SLOT(save()));

#ifndef MULTI_DATA_SET
    /******** Graph add ********/
    action = menu->addAction(QIcon(":/icons/chart-add"), "Add Graph",
                                      this, SLOT(addGraph()));

    if (plotter->availableColumns(0) == 0) {
        // All graphs are already in the chart. No more to add.
        action->setEnabled(false);
    }

#ifdef CUSTOMISE_CHART
    menu->addSeparator();
    // The customise chart window isn't finished yet.
    menu->addAction("Customise",
                    this, SLOT(customiseChart()));
#endif
#endif // MULTI_DATA_SET

    /******** Plot feature visibility & layout ********/
    menu->addSeparator();

#ifdef MULTI_DATA_SET
    if (dataSets.count() > 1) {
        QMenu* rescaleMenu = menu->addMenu("&Rescale");
        rescaleMenu->addAction("By &Time", plotter.data(),
                               SLOT(rescaleByTime()));
        rescaleMenu->addAction("By Time of &Year", plotter.data(),
                               SLOT(rescaleByTimeOfYear()));
        rescaleMenu->addAction("By Time of &Day", plotter.data(),
                               SLOT(rescaleByTimeOfDay()));
    }
#else
    action = menu->addAction("&Rescale", plotter.data(),
                             SLOT(rescaleByTime()));
#endif // MULTI_DATA_SET

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

    menu->addSeparator();

#ifdef MULTI_DATA_SET
    // X Axis lock
    action = menu->addAction("Lock &X Axis", this, SLOT(toggleXAxisLock()));
    action->setCheckable(true);
    action->setChecked(ui->cbXLock->isChecked());
#endif
    // Y Axis lock
    action = menu->addAction("Lock &Y Axis", this, SLOT(toggleYAxisLock()));
    action->setCheckable(true);
    action->setChecked(ui->cbYLock->isChecked());


    /******** Finished ********/
    menu->popup(ui->chart->mapToGlobal(point));
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

void ChartWindow::showKeyAxisContextMenu(QPoint point, QCPAxis *axis) {

    // Deselect all X axis
    foreach(QCPAxis* axis, ui->chart->axisRect()->axes(QCPAxis::atTop | QCPAxis::atBottom)) {
        axis->setSelectedParts(QCPAxis::spNone);
    }

    // Select the axis that was right clicked on
    axis->setSelectedParts(QCPAxis::spAxis | QCPAxis::spTickLabels |
                           QCPAxis::spAxisLabel);
    ui->chart->replot();

    QMenu* menu = new QMenu(this);
    menu->setAttribute(Qt::WA_DeleteOnClose);

    menu->addAction("&Rename...",
                    this, SLOT(renameSelectedKeyAxis()));

    menu->addAction("&Add Graph...", this, SLOT(addGraph()));

    menu->addAction("&Change Timespan...",
                    this, SLOT(changeSelectedKeyAxisTimespan()));

    menu->addSeparator();

    QAction* action = menu->addAction("Remove &Data Set",
                                      this, SLOT(removeSelectedKeyAxis()));

    if (dataSets.count() == 1) {
        // Don't allow the last dataset to be removed.
        action->setEnabled(false);
    }

    menu->popup(ui->chart->mapToGlobal(point));
}

void ChartWindow::renameSelectedKeyAxis() {    
    QCPAxis* keyAxis = 0;

    foreach(QCPAxis* axis, ui->chart->axisRect()->axes(QCPAxis::atTop | QCPAxis::atBottom)) {
        if (axis->selectedParts().testFlag(QCPAxis::spAxis) ||
                axis->selectedParts().testFlag(QCPAxis::spTickLabels))
            keyAxis = axis;
    }

    if (keyAxis == 0) {
        return;
    }

    bool ok;
    QString name = QInputDialog::getText(
                this,
                "Rename Axis",
                "New Axis Label:",
                QLineEdit::Normal,
                keyAxis->label(),
                &ok);
    if (ok) {
        keyAxis->setLabel(name);
        ui->chart->replot();
    }
}

void ChartWindow::changeSelectedKeyAxisTimespan() {
    QCPAxis* keyAxis = 0;

    foreach(QCPAxis* axis, ui->chart->axisRect()->axes(QCPAxis::atTop | QCPAxis::atBottom)) {
        if (axis->selectedParts().testFlag(QCPAxis::spAxis) ||
                axis->selectedParts().testFlag(QCPAxis::spTickLabels))
            keyAxis = axis;
    }

    if (keyAxis == 0) {
        return;
    }

    dataset_id_t dataset = keyAxis->property(AXIS_DATASET).toInt();

    int index = -1;
    for (int i = 0; i < dataSets.count(); i++) {
        if (dataSets[i].id == dataset) {
            index = i;
        }
    }
    if (index < 0) {
        return;
    }

    DataSetTimespanDialog dlg(this);
    dlg.setTime(dataSets[index].startTime, dataSets[index].endTime);
    int result = dlg.exec();

    if (result == QDialog::Accepted) {
        QDateTime start = dlg.getStartTime();
        QDateTime end = dlg.getEndTime();

        qDebug() << "Changing timespan for dataset" << dataset
                 << "to:" << start << "-" << end;

        dataSets[index].startTime = start;
        dataSets[index].endTime = end;
        plotter->changeDataSetTimespan(dataset, start, end);
    }
}

void ChartWindow::removeSelectedKeyAxis() {
    QCPAxis* keyAxis = 0;

    foreach(QCPAxis* axis, ui->chart->axisRect()->axes(QCPAxis::atTop | QCPAxis::atBottom)) {
        if (axis->selectedParts().testFlag(QCPAxis::spAxis) ||
                axis->selectedParts().testFlag(QCPAxis::spTickLabels))
            keyAxis = axis;
    }

    if (keyAxis == 0) {
        return;
    }

    dataset_id_t dataset = keyAxis->property(AXIS_DATASET).toInt();

    plotter->removeGraphs(dataset, ALL_SAMPLE_COLUMNS);

    int index = -1;
    for (int i = 0; i < dataSets.count(); i++) {
        if (dataSets[i].id == dataset) {
            index = i;
        }
    }
    if (index > -1) {
        dataSets.removeAt(index);
    }
}

//void ChartWindow::showValueAxisContextMenu(QPoint point, QCPAxis *axis) {

//    axis->setSelectedParts(QCPAxis::spAxis | QCPAxis::spTickLabels |
//                           QCPAxis::spAxisLabel);
//    ui->chart->replot();

//    QMenu* menu = new QMenu(this);
//    menu->setAttribute(Qt::WA_DeleteOnClose);

//    menu->addAction("&Rename",
//                    this, SLOT(renameSelectedValueAxis()));
//    menu->addAction("&Move to Opposite",
//                    this, SLOT(moveSelectedValueAxisToOpposite()));

//    menu->popup(ui->chart->mapToGlobal(point));
//}



//void ChartWindow::moveSelectedValueAxisToOpposite() {
//    QList<QCPAxis*> valueAxes = ui->chart->axisRect()->axes(QCPAxis::atLeft |
//                                                          QCPAxis::atRight);
//    foreach (QCPAxis* valueAxis, valueAxes) {
//        if (valueAxis->selectTest(point, false) >= 0) {
//            valueAxis->axisType()
//            return;
//        }
//    }
//}


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
        addTitle(plotTitleValue);
        ui->chart->replot();
    }
}

void ChartWindow::addTitle(QString title) {
    plotTitleEnabled = true;
    plotTitleValue = title;
    ui->chart->plotLayout()->insertRow(0);
    plotTitle = new QCPPlotTitle(ui->chart, title);
    plotTitle->setTextColor(plotTitleColour);
    ui->chart->plotLayout()->addElement(0, 0, plotTitle);
}

void ChartWindow::removeTitle(bool replot)
{
    plotTitleEnabled = false;
    ui->chart->plotLayout()->remove(plotTitle);
    ui->chart->plotLayout()->simplify();

    if (replot) {
        ui->chart->replot();
    }
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

    QList<QCPAxis*> axes = ui->chart->axisRect()->axes();

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

        dataset_id_t dataset = graph->property(GRAPH_DATASET).toInt();
        SampleColumn column = (SampleColumn)graph->property(GRAPH_TYPE).toInt();

        // Turn off the column so it doesn't come back when the user
        // hits refresh.
        plotter->removeGraph(dataset, column);

        // Turn the column off in the dataset itself so it doesn't come back
        // later.
        for (int i = 0; i < dataSets.count(); i++) {
            if (dataSets[i].id == dataset) {
                dataSets[i].columns &= ~column;
            }
        }
    }
}

void ChartWindow::renameSelectedGraph()
{
    if (!ui->chart->selectedGraphs().isEmpty()) {
        QCPGraph* graph = ui->chart->selectedGraphs().first();

        bool ok;
        QString title = QInputDialog::getText(
                    this,
                    "Rename Graph",
                    "New graph name:",
                    QLineEdit::Normal,
                    graph->name(),
                    &ok);

        if (!title.isNull() && ok) {
            graph->setName(title);

            // Save the new name in in the graph style settings so it survives
            // reloads
            plotter->getStyleForGraph(graph).setName(title);

            ui->chart->replot();
        }
    }
}

void ChartWindow::changeGraphStyle(QCPGraph* graph) {
    if (graph == NULL) {
        qWarning() << "NULL graph while attempting to change style";
        return;
    }

    GraphStyle& style = plotter->getStyleForGraph(graph);

    // The Graph Style Dialog updates the style directly on accept.
    GraphStyleDialog gsd(style, this);
    int result = gsd.exec();
    if (result == QDialog::Accepted) {
        style.applyStyle(graph);
        ui->chart->replot();
    }
}

void ChartWindow::changeSelectedGraphStyle()
{
    if (!ui->chart->selectedGraphs().isEmpty()) {
        QCPGraph* graph = ui->chart->selectedGraphs().first();
        changeGraphStyle(graph);
    }
}


void ChartWindow::plottableDoubleClick(QCPAbstractPlottable* plottable,
                                       QMouseEvent* event) {

    QCPGraph *graph = qobject_cast<QCPGraph *>(plottable);

    if (graph == 0) {
        // Its not a QCPGraph. Whatever it is we don't currently support
        // customising its style
    }

    changeGraphStyle(graph);
}

void ChartWindow::legendDoubleClick(QCPLegend* /*legend*/,
                                    QCPAbstractLegendItem* item,
                                    QMouseEvent* /*event*/) {
    if (item == NULL)  {
        // The legend itself was double-clicked. Don't care.
        return;
    }

    QCPPlottableLegendItem  *plottableItem =
            qobject_cast<QCPPlottableLegendItem *>(item);
    if (plottableItem == NULL) {
        // Some other legend item we don't care about.
        return;
    }

    QCPAbstractPlottable *plottable = plottableItem->plottable();

    QCPGraph* graph = qobject_cast<QCPGraph *>(plottable);

    if (graph == NULL) {
        // Sorry, we only support customising graphs.
        return;
    }

    changeGraphStyle(graph);
}


QList<QCPAxis*> ChartWindow::valueAxes() {
    return ui->chart->axisRect()->axes(QCPAxis::atLeft | QCPAxis::atRight);
}

void ChartWindow::addGraph()
{
    dataset_id_t dataset = FIRST_DATA_SET;

#ifdef MULTI_DATA_SET
    // Find the axis that was right-clicked on and grab the data set id.
    QCPAxis* keyAxis = 0;

    foreach(QCPAxis* axis, ui->chart->axisRect()->axes(QCPAxis::atTop | QCPAxis::atBottom)) {
        if (axis->selectedParts().testFlag(QCPAxis::spAxis) ||
                axis->selectedParts().testFlag(QCPAxis::spTickLabels))
            keyAxis = axis;
    }

    if (keyAxis == 0) {
        return;
    }

    dataset = keyAxis->property(AXIS_DATASET).toInt();

#endif

    AddGraphDialog adg(plotter->availableColumns(dataset),
                       solarDataAvailable,
                       hw_type,
                       this);
    if (adg.exec() == QDialog::Accepted) {
        plotter->addGraphs(dataset, adg.selectedColumns());
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

void ChartWindow::customiseChart() {

    QMap<SampleColumn, GraphStyle> originalStyles = plotter->getGraphStyles(FIRST_DATA_SET);

    CustomiseChartDialog ccd(originalStyles, solarDataAvailable, plotTitleEnabled,
                             plotTitleValue, plotTitleColour, plotBackgroundBrush,
                             this);
    if (ccd.exec() == QDialog::Accepted) {
        QMap<SampleColumn, GraphStyle> newStyles = ccd.getGraphStyles();

        qDebug() << "Customise Chart Accept.";

        bool replotRequired = false;

        foreach (SampleColumn column, originalStyles.keys()) {
            GraphStyle original = originalStyles[column];
            GraphStyle updated = newStyles[column];

            qDebug() << "Checking column " << column;

            if (updated != original) {
                qDebug() << "Column settings changed.";
                // Find the graph and restyle it.

                for (int i = 0; i < ui->chart->graphCount(); i++) {
                    QCPGraph* graph = ui->chart->graph(i);
                    if (graph->property(GRAPH_TYPE).toInt() == column) {
                        qDebug() << "Graph found. Restyling.";
                        updated.applyStyle(graph);
                    }
                }

                replotRequired = true;
            }
        }
        plotter->setGraphStyles(newStyles, FIRST_DATA_SET);

        if (ccd.getTitleEnabled()) {
            QString title = ccd.getTitle();
            QColor colour = ccd.getTitleColour();

            if (title != plotTitleValue || colour != plotTitleColour) {
                if (plotTitleEnabled) {
                    removeTitle(false);
                }
                plotTitleColour = colour;
                addTitle(title);
                replotRequired = true;
            }
        } else {
            removeTitle();
        }

        QBrush newBackgroundBrush = ccd.getBackgroundBrush();
        if (newBackgroundBrush != plotBackgroundBrush) {
            plotBackgroundBrush = newBackgroundBrush;
            ui->chart->axisRect()->setBackground(newBackgroundBrush);
            replotRequired = true;
        }

        if (replotRequired)
            ui->chart->replot();
    }
}

void ChartWindow::addDataSet() {
    ChartOptionsDialog options(solarDataAvailable, hw_type);
    options.setWindowTitle("Add Data Set");

    int result = options.exec();
    if (result != QDialog::Accepted)
        return; // User canceled. Nothing to do.

    SampleColumns columns = options.getColumns();

    DataSet ds;
    ds.columns = columns;
    ds.startTime = options.getStartTime();
    ds.endTime = options.getEndTime();
    ds.aggregateFunction = options.getAggregateFunction();
    ds.groupType = options.getAggregateGroupType();
    ds.customGroupMinutes = options.getCustomMinutes();
    ds.id = dataSets.count();
    dataSets.append(ds);


    // This will resuilt in the chart being redrawn completely.
    // TODO: don't rebuild the entire chart. Just add the new data set.
    reloadDataSets(true);
}
