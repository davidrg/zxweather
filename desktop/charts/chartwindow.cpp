#include "chartwindow.h"
#include "ui_chartwindow.h"
#include "settings.h"
#include "addgraphdialog.h"
#include "graphstyledialog.h"
#include "datasettimespandialog.h"
#include "datasetsdialog.h"
#include "axistickformatdialog.h"

#include "datasource/webdatasource.h"
#include "datasource/databasedatasource.h"
#include "datasource/dialogprogresslistener.h"

#include <QFileDialog>
#include <QFontDialog>
#include <QtDebug>
#include <QPen>
#include <QBrush>
#include <QMessageBox>
#include <QInputDialog>
#include <QMenu>
#include <QIcon>
#include <QClipboard>
#include <QTimer>
#include <QSvgGenerator>

// single-data-set functionality needs to know which the first (and only) data
// set is.
#define FIRST_DATA_SET 0

// This enables multi-data-set support in the UI,
#define MULTI_DATA_SET

ChartWindow::ChartWindow(QList<DataSet> dataSets, bool solarAvailable, bool isWireless,
                         QWidget *parent) :
    QMainWindow(parent),
    ui(new Ui::ChartWindow)
{
    ui->setupUi(this);

    Settings& settings = Settings::getInstance();

    mouseTracker = new ChartMouseTracker(ui->chart);
    mouseTracker->setEnabled(settings.chartTracksMouseEnabled());
    ui->action_Track_Cursor->setChecked(mouseTracker->isEnabled());

    restoreGeometry(Settings::getInstance().chartWindowGeometry());
    restoreState(Settings::getInstance().chartWindowState());

    solarDataAvailable = solarAvailable;
    this->isWireless = isWireless;

    gridVisible = true;
    ui->action_Grid->setChecked(gridVisible);

    plotTitleEnabled = false;
    nextDataSetId = 0;

    basicInteractionManager.reset(
            new BasicQCPInteractionManager(ui->chart, this));

    plotter.reset(new WeatherPlotter(ui->chart, this));

    // These will be turned back on later if they are needed.
    ui->actionLock_X_Axes->setVisible(false);
    ui->actionLock_Y_Axes->setVisible(false);
    setYAxisLock();
    setXAxisLock();

    plotter->setCursorEnabled(settings.chartCursorEnabled());
    ui->actionC_ursor->setChecked(plotter->isCursorEnabled());

    // Hide the cursor while zooming (the tags drift with the zoom otherwise)
    connect(basicInteractionManager.data(), SIGNAL(zooming()),
            plotter.data(), SLOT(hideCursor()));

    ChartColours colours = settings.getChartColours();
    plotTitleColour = colours.title;
    plotBackgroundBrush = QBrush(colours.background);
    plotTitleFont = settings.defaultChartTitleFont();

    ui->chart->legend->setFont(settings.defaultChartLegendFont());

    ui->chart->axisRect()->setBackground(plotBackgroundBrush);

    AbstractDataSource *ds;
    if (settings.sampleDataSourceType() == Settings::DS_TYPE_DATABASE)
        ds = new DatabaseDataSource(new DialogProgressListener(this), this);
    else
        ds = new WebDataSource(new DialogProgressListener(this), this);

    hw_type = ds->getHardwareType();
    extraColumns = ds->extraColumnsAvailable();
    extraColumnNames = ds->extraColumnNames();
    plotter->setDataSource(ds);

    // Toolbar - Main
    connect(ui->actionAdd_Dataset, SIGNAL(triggered()), this, SLOT(addDataSet()));
    connect(ui->actionDatasets, SIGNAL(triggered()), this, SLOT(showDataSetsWindow()));
    // ---
    connect(ui->action_Title, SIGNAL(triggered()), this, SLOT(toggleTitle()));
    connect(ui->action_Legend, SIGNAL(triggered()), this, SLOT(showLegendToggle()));
    connect(ui->action_Grid, SIGNAL(triggered()), this, SLOT(showGridToggle()));
    // ---
    connect(ui->actionLock_X_Axes, SIGNAL(triggered()), this, SLOT(setXAxisLock()));
    connect(ui->actionLock_Y_Axes, SIGNAL(triggered()), this, SLOT(setYAxisLock()));
#ifdef FEATURE_PLUS_CURSOR
    connect(ui->actionC_ursor, SIGNAL(triggered()), this, SLOT(toggleCursor()));
#endif
    connect(ui->action_Track_Cursor, SIGNAL(triggered(bool)), this, SLOT(setMouseTrackingEnabled(bool)));

    // ---
    connect(ui->action_Save, SIGNAL(triggered()), this, SLOT(save()));
    connect(ui->action_Copy, SIGNAL(triggered()), this, SLOT(copy()));

    // Toolbar - Selected Graph
    connect(ui->action_Rename_Graph, SIGNAL(triggered()), this, SLOT(renameSelectedGraph()));
    connect(ui->actionC_hange_Style, SIGNAL(triggered()), this, SLOT(changeSelectedGraphStyle()));
    connect(ui->action_Remove, SIGNAL(triggered()), this, SLOT(removeSelectedGraph()));

    connect(basicInteractionManager.data(), SIGNAL(graphSelected(bool)),
                this, SLOT(setGraphActionsEnabled(bool)));
    setGraphActionsEnabled(false);

    // Toolbar - Selected Dataset
    connect(ui->action_Add_Graph, SIGNAL(triggered()), this, SLOT(addGraph()));
    connect(ui->actionC_hange_Timespan, SIGNAL(triggered()),
            this, SLOT(changeSelectedKeyAxisTimespan()));

    connect(basicInteractionManager.data(), SIGNAL(keyAxisSelected(bool)),
            this, SLOT(setDataSetActionsEnabled(bool)));
    setDataSetActionsEnabled(false);


    // WeatherPlotter events
    connect(plotter.data(), SIGNAL(axisCountChanged(int, int)),
            this, SLOT(chartAxisCountChanged(int, int)));
    connect(plotter.data(), SIGNAL(dataSetRemoved(dataset_id_t)),
            this, SLOT(dataSetRemoved(dataset_id_t)));
    connect(plotter.data(), SIGNAL(legendVisibilityChanged(bool)),
            ui->action_Legend, SLOT(setChecked(bool)));

    // Chart events
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
    connect(ui->chart, SIGNAL(plottableDoubleClick(QCPAbstractPlottable*,int,QMouseEvent*)),
            this, SLOT(plottableDoubleClick(QCPAbstractPlottable*,int,QMouseEvent*)));
    connect(ui->chart,
            SIGNAL(legendDoubleClick(QCPLegend*,QCPAbstractLegendItem*,
                                     QMouseEvent*)),
            this,
            SLOT(legendDoubleClick(QCPLegend*,QCPAbstractLegendItem*,
                                   QMouseEvent*)));

    setWindowTitle(tr("Chart"));

    this->dataSets = dataSets;

    for(int i = 0; i < this->dataSets.count(); i++) {
        this->dataSets[i].id = i;
    }

    reloadDataSets(true);
}

ChartWindow::~ChartWindow()
{
    delete ui;
}

void ChartWindow::closeEvent(QCloseEvent *event) {

    Settings::getInstance().saveChartWindowGeometry(saveGeometry());
    Settings::getInstance().saveChartWindowState(saveState());

    QMainWindow::closeEvent(event);
}

void ChartWindow::setMouseTrackingEnabled(bool enabled) {
    mouseTracker->setEnabled(enabled);
    Settings::getInstance().setChartTracksMouseEnabled(enabled);
}

void ChartWindow::setGraphActionsEnabled(bool enabled) {
    ui->action_Rename_Graph->setEnabled(enabled);
    ui->actionC_hange_Style->setEnabled(enabled);
    ui->action_Remove->setEnabled(enabled);
}

void ChartWindow::setDataSetActionsEnabled(bool enabled) {
    bool graphsAvailable = false;
    dataset_id_t ds = getSelectedDataset();
    if (ds == INVALID_DATASET_ID)
        enabled = false;
    else {
        SampleColumns selected = plotter->selectedColumns(ds);
        SampleColumns supported = AddGraphDialog::supportedColumns(hw_type, isWireless, solarDataAvailable, extraColumns);
        graphsAvailable = selected.standard != supported.standard || selected.extra != supported.extra;
    }

    ui->action_Add_Graph->setEnabled(enabled && graphsAvailable);
    ui->actionC_hange_Timespan->setEnabled(enabled);
}

void ChartWindow::reloadDataSets(bool rebuildChart) {

    dataset_id_t max_id = 0;
    foreach (DataSet ds, dataSets) {
        if (ds.id > max_id) {
            max_id = ds.id;
        }
    }
    nextDataSetId = max_id+1;

    if (rebuildChart) {
        plotter->drawChart(dataSets);
    }

    if (dataSets.count() == 1 && !dataSets.first().title.isEmpty()) {
        addTitle(dataSets.first().title);
    }
}

void ChartWindow::dataSetRemoved(dataset_id_t dataSetId) {
    for(int i = 0; i < dataSets.count(); i++) {
        if (dataSets[i].id == dataSetId) {
            dataSets.removeAt(i);
            emit dataSetWasRemoved(dataSetId);
            return;
        }
    }
    qWarning() << "Could not find removed data set" << dataSetId;
}

void ChartWindow::chartAxisCountChanged(int valueAxes, int keyAxes) {
    ui->actionLock_Y_Axes->setVisible(valueAxes > 1);
    ui->actionLock_X_Axes->setVisible(keyAxes > 1);
}

void ChartWindow::setYAxisLock() {
    basicInteractionManager->setYAxisLockEnabled(
                ui->actionLock_Y_Axes->isEnabled() && ui->actionLock_Y_Axes->isChecked());
    ui->chart->deselectAll();
    ui->chart->replot();
}

void ChartWindow::toggleYAxisLock() {
    ui->actionLock_Y_Axes->setChecked(!ui->actionLock_Y_Axes->isChecked());
    setYAxisLock();
}

void ChartWindow::setXAxisLock() {
    basicInteractionManager->setXAxisLockEnabled(
                ui->actionLock_X_Axes->isEnabled() && ui->actionLock_X_Axes->isChecked());
    ui->chart->deselectAll();
    ui->chart->replot();
}

void ChartWindow::toggleXAxisLock() {
    ui->actionLock_X_Axes->setChecked(!ui->actionLock_X_Axes->isChecked());
    setXAxisLock();
}

void ChartWindow::axisDoubleClick(QCPAxis *axis, QCPAxis::SelectablePart part,
                                  QMouseEvent *event)
{
    Q_UNUSED(event)

    // If the user double-clicked on the axis label then ask for new
    // label text.
    if (part == QCPAxis::spAxisLabel) {
        QString defaultLabel = plotter->defaultLabelForAxis(axis);
        bool ok;

        QString newLabel = QInputDialog::getText(
                    this,
                    QString(tr("%1 Axis Label")).arg(defaultLabel),
                    tr("New axis label:"),
                    QLineEdit::Normal,
                    axis->label(),
                    &ok);
        if (ok) {
            axis->setLabel(newLabel);
            ui->chart->replot();
        }
    }
}

void ChartWindow::textElementDoubleClick(QMouseEvent *event)
{
    Q_UNUSED(event)
    if (QCPTextElement *element = qobject_cast<QCPTextElement*>(sender())) {
        bool ok;
        QString newTitle = QInputDialog::getText(
                    this,
                    tr("Change Title"),
                    tr("New title:"),
                    QLineEdit::Normal,
                    element->text(),
                    &ok);

        if (ok) {
            if (newTitle.isEmpty()) {
                plotTitleValue = QString();
                setWindowTitle(tr("Untitled - Chart"));
                QTimer::singleShot(1, this, SLOT(removeTitle()));
                return;
            }
            element->setText(newTitle);
            plotTitleValue = newTitle;
            ui->chart->replot();
        }
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

    if (plotTitleEnabled && plotTitle->selectTest(point, false) >= 0 && plotTitle->visible()) {
        showTitleContextMenu(point);
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

    // Check if a Y axis was right-clicked on
    QList<QCPAxis*> valueAxes = ui->chart->axisRect()->axes(QCPAxis::atLeft |
                                                          QCPAxis::atRight);
    foreach (QCPAxis* valueAxis, valueAxes) {
        if (valueAxis->selectTest(point, false) >= 0) {
            showValueAxisContextMenu(point, valueAxis);
            return;
        }
    }

    showChartContextMenu(point);
}


void ChartWindow::showChartContextMenu(QPoint point) {
    QMenu* menu = new QMenu(this);
    menu->setAttribute(Qt::WA_DeleteOnClose);

    QAction *action;

#ifdef MULTI_DATA_SET
    menu->addAction(tr("Add Data Set..."),
                    this, SLOT(addDataSet()));

    menu->addAction(tr("Data Sets..."),
                    this, SLOT(showDataSetsWindow()));

    // If a graph is currently selected show some extra options.
    if (!ui->chart->selectedGraphs().isEmpty()) {
        QMenu* graphMenu = menu->addMenu(tr("&Selected Graph"));
        graphMenu->addAction(tr("&Rename..."), this, SLOT(renameSelectedGraph()));
        graphMenu->addAction(tr("&Change Style..."), this,
                             SLOT(changeSelectedGraphStyle()));
        graphMenu->addSeparator();
        graphMenu->addAction(tr("R&emove"), this, SLOT(removeSelectedGraph()));
    }

#else
    /******** Graph remove ********/

    // If a graph is currently selected let it be removed.
    if (!ui->chart->selectedGraphs().isEmpty()) {
        menu->addAction(tr("Remove selected graph"),
                        this, SLOT(removeSelectedGraph()));
    }
#endif

    menu->addSeparator();
    menu->addAction(QIcon(":/icons/save"), tr("&Save..."), this, SLOT(save()));
    menu->addAction(tr("&Copy"), this, SLOT(copy()));
#ifndef MULTI_DATA_SET
    /******** Graph add ********/
    action = menu->addAction(QIcon(":/icons/chart-add"), tr("Add Graph..."),
                                      this, SLOT(addGraph()));

    if (plotter->availableColumns(0) == 0) {
        // All graphs are already in the chart. No more to add.
        action->setEnabled(false);
    }
#endif // MULTI_DATA_SET

    /******** Plot feature visibility & layout ********/
    menu->addSeparator();

    menu->addAction(tr("&Rescale"), plotter.data(), SLOT(rescale()));

#ifdef MULTI_DATA_SET
    if (dataSets.count() > 1) {
        QMenu* rescaleMenu = menu->addMenu(tr("&Align X Axes"));
        WeatherPlotter::RescaleType type = plotter->getCurrentScaleType();
        QAction *act = rescaleMenu->addAction(tr("By &Time"), plotter.data(),
                               SLOT(rescaleByTime()));
        act->setCheckable(true);
        act->setChecked(type == WeatherPlotter::RS_YEAR);
        act = rescaleMenu->addAction(tr("By Time of &Year"), plotter.data(),
                               SLOT(rescaleByTimeOfYear()));
        act->setCheckable(true);
        act->setChecked(type == WeatherPlotter::RS_MONTH);
        act = rescaleMenu->addAction(tr("By Time of &Day"), plotter.data(),
                               SLOT(rescaleByTimeOfDay()));
        act->setCheckable(true);
        act->setChecked(type == WeatherPlotter::RS_TIME);
    }
#else
    action = menu->addAction(tr("&Align X Axes"), plotter.data(),
                             SLOT(rescaleByTime()));
#endif // MULTI_DATA_SET

    // Title visibility option.
    action = menu->addAction(tr("Show Title"),
                             this, SLOT(showTitleToggle()));
    action->setCheckable(true);
    action->setChecked(!plotTitle.isNull());


    // Legend visibility option.
    action = menu->addAction(tr("Show Legend"),
                             this, SLOT(showLegendToggle()));
    action->setCheckable(true);
    action->setChecked(ui->chart->legend->visible());

    // Grid visibility option
    action = menu->addAction(tr("Show Grid"),
                             this,
                             SLOT(showGridToggle()));
    action->setCheckable(true);
    action->setChecked(gridVisible);

    menu->addSeparator();

#ifdef MULTI_DATA_SET
    // X Axis lock
    action = menu->addAction(tr("Lock &X Axis"), this, SLOT(toggleXAxisLock()));
    action->setCheckable(true);
    action->setChecked(ui->actionLock_X_Axes->isChecked());
#endif
    // Y Axis lock
    action = menu->addAction(tr("Lock &Y Axis"), this, SLOT(toggleYAxisLock()));
    action->setCheckable(true);
    action->setChecked(ui->actionLock_Y_Axes->isChecked());

#ifdef FEATURE_PLUS_CURSOR
    action = menu->addAction(tr("Enable Crosshair"), this, SLOT(toggleCursor()));
    action->setCheckable(true);
    action->setChecked(plotter->isCursorEnabled());
#endif

    /******** Finished ********/
    menu->popup(ui->chart->mapToGlobal(point));
}

#ifdef FEATURE_PLUS_CURSOR
void ChartWindow::toggleCursor() {
    bool enabled = !plotter->isCursorEnabled();
    Settings::getInstance().setChartCursorEnabled(enabled);
    plotter->setCursorEnabled(enabled);
    ui->actionC_ursor->setChecked(plotter->isCursorEnabled());
}
#endif

void ChartWindow::showLegendContextMenu(QPoint point)
{
    QMenu *menu = new QMenu(this);
    menu->setAttribute(Qt::WA_DeleteOnClose);

    for (int i = 0; i < ui->chart->legend->itemCount(); i++) {
        if (ui->chart->legend->item(i)->selectTest(point, false) >= 0) {
           QCPAbstractLegendItem *item = ui->chart->legend->item(i);
           QCPPlottableLegendItem *plottableItem = qobject_cast<QCPPlottableLegendItem*>(item);
           if (plottableItem != NULL) {               
               // Deselect any currently selected plottables
               for (int i = 0; i < ui->chart->plottableCount(); i++) {
                   ui->chart->plottable(i)->setSelection(QCPDataSelection(QCPDataRange(0, 0)));
               }

               // select the graph
               plottableItem->setSelected(true);
               plottableItem->plottable()->setSelection(QCPDataSelection(QCPDataRange(0,1)));

               ui->chart->replot(QCustomPlot::rpRefreshHint);

               // And add on the graphs context menu options
               menu->addAction(tr("&Rename..."), this, SLOT(renameSelectedGraph()));
               menu->addAction(tr("&Change Style..."), this,
                                    SLOT(changeSelectedGraphStyle()));
               menu->addAction(tr("R&emove"), this, SLOT(removeSelectedGraph()));

               menu->addSeparator();
               // We've found the legend item that was right-clicked - no need to search
               // any further.
               break;
           }
        }
    }

    menu->addAction(tr("&Change Font..."), this, SLOT(changeLegendFont()));
    menu->addSeparator();

    // Options to re-position the legend
    menu->addAction(tr("Move to top left"),
                    this,
                    SLOT(moveLegend()))->setData((int)(Qt::AlignTop
                                                       | Qt::AlignLeft));
    menu->addAction(tr("Move to top center"),
                    this,
                    SLOT(moveLegend()))->setData((int)(Qt::AlignTop
                                                       | Qt::AlignHCenter));
    menu->addAction(tr("Move to top right"),
                    this,
                    SLOT(moveLegend()))->setData((int)(Qt::AlignTop
                                                       | Qt::AlignRight));
    menu->addAction(tr("Move to bottom right"),
                    this,
                    SLOT(moveLegend()))->setData((int)(Qt::AlignBottom
                                                       | Qt::AlignRight));
    menu->addAction(tr("Move to bottom center"),
                    this,
                    SLOT(moveLegend()))->setData((int)(Qt::AlignBottom
                                                       | Qt::AlignHCenter));
    menu->addAction(tr("Move to bottom left"),
                    this,
                    SLOT(moveLegend()))->setData((int)(Qt::AlignBottom
                                                       | Qt::AlignLeft));

    // And an option to get rid of it entirely.
    menu->addSeparator();
    menu->addAction(tr("Hide"), this, SLOT(showLegendToggle()));

    menu->popup(ui->chart->mapToGlobal(point));
}

void ChartWindow::showTitleContextMenu(QPoint point) {
    QMenu *menu = new QMenu(this);
    menu->setAttribute(Qt::WA_DeleteOnClose);

    QAction* act = menu->addAction(tr("&Edit..."), this, SLOT(editTitle()));
    QFont f = act->font();
    f.setBold(true);
    act->setFont(f);

    menu->addAction(tr("&Change font..."), this, SLOT(changeTitleFont()));
    menu->addSeparator();
    menu->addAction(tr("&Hide"), this, SLOT(removeTitle()));

    menu->popup(ui->chart->mapToGlobal(point));
}

void ChartWindow::changeTitleFont() {
    bool ok;
    QFont newFont = QFontDialog::getFont(&ok, plotTitleFont, this, tr("Title Font"));

    if (ok) {
        plotTitle->setFont(newFont);
        plotTitleFont = newFont;
        ui->chart->replot();
    }
}

void ChartWindow::editTitle() {
    bool ok;
    QString newTitle = QInputDialog::getText(
                this,
                tr("Change Title"),
                tr("New title:"),
                QLineEdit::Normal,
                plotTitle->text(),
                &ok);

    if (ok) {
        if (newTitle.isEmpty()) {
            plotTitleValue = QString();
            setWindowTitle(tr("Untitled - Chart"));
            QTimer::singleShot(1, this, SLOT(removeTitle()));
            return;
        }
        plotTitle->setText(newTitle);
        ui->chart->replot();
        plotTitleValue = newTitle;
    }
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

    menu->addAction(tr("&Rename..."),
                    this, SLOT(renameSelectedKeyAxis()));
    menu->addAction(tr("&Hide Axis"),
                    this, SLOT(hideSelectedKeyAxis()));

    menu->addAction(tr("&Change Label Font..."), this, SLOT(changeAxisLabelFont()));
    menu->addAction(tr("&Change Tick Font..."), this, SLOT(changeAxisTickLabelFont()));
    menu->addAction(tr("&Change Tick Format..."), this, SLOT(setSelectedKeyAxisTickFormat()));

    menu->addSeparator();

    bool graphsAvailable = false;
    dataset_id_t ds = getSelectedDataset();
    if (ds != INVALID_DATASET_ID) {
        SampleColumns selected = plotter->selectedColumns(ds);
        SampleColumns supported = AddGraphDialog::supportedColumns(hw_type, isWireless, solarDataAvailable, extraColumns);
        graphsAvailable = selected.standard != supported.standard || selected.extra != supported.extra;
    }
    QAction *act = menu->addAction(QIcon(":/icons/chart-add"),tr("&Add Graph..."), this, SLOT(addGraph()));
    act->setEnabled(graphsAvailable);

    menu->addAction(QIcon(":/icons/timespan"),tr("&Change Timespan..."),
                    this, SLOT(changeSelectedKeyAxisTimespan()));
    menu->addAction(tr("Refresh"), this, SLOT(refreshSelectedKeyAxis()));

    menu->addSeparator();

    QAction* action = menu->addAction(tr("Remove &Data Set"),
                                      this, SLOT(removeSelectedKeyAxis()));

    if (dataSets.count() == 1) {
        // Don't allow the last dataset to be removed.
        action->setEnabled(false);
    }

    menu->popup(ui->chart->mapToGlobal(point));
}

void ChartWindow::changeAxisLabelFont() {
    QCPAxis* selectedAxis = 0;

    foreach(QCPAxis* axis, ui->chart->axisRect()->axes()) {
        if (axis->selectedParts().testFlag(QCPAxis::spAxis) ||
                axis->selectedParts().testFlag(QCPAxis::spTickLabels))
            selectedAxis = axis;
    }

    if (selectedAxis != NULL) {
        QFont current = selectedAxis->labelFont();
        bool ok;
        QFont newFont = QFontDialog::getFont(&ok, current, this, tr("Axis Label Font"));

        if (ok) {
            selectedAxis->setLabelFont(newFont);
            ui->chart->replot();
        }
    }
}

void ChartWindow::changeAxisTickLabelFont() {
    QCPAxis* selectedAxis = 0;

    foreach(QCPAxis* axis, ui->chart->axisRect()->axes()) {
        if (axis->selectedParts().testFlag(QCPAxis::spAxis) ||
                axis->selectedParts().testFlag(QCPAxis::spTickLabels))
            selectedAxis = axis;
    }

    if (selectedAxis != NULL) {
        QFont current = selectedAxis->tickLabelFont();
        bool ok;
        QFont newFont = QFontDialog::getFont(&ok, current, this, tr("Axis Tick Label Font"));

        if (ok) {
            selectedAxis->setTickLabelFont(newFont);
            ui->chart->replot();
        }
    }
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
                tr("Rename Axis"),
                tr("New Axis Label:"),
                QLineEdit::Normal,
                keyAxis->label(),
                &ok);
    if (ok) {
        keyAxis->setLabel(name);
        emit dataSetRenamed(keyAxis->property(AXIS_DATASET).toInt(), name);
        ui->chart->replot();
    }
}

void ChartWindow::hideSelectedKeyAxis() {
    QCPAxis* keyAxis = 0;

    foreach(QCPAxis* axis, ui->chart->axisRect()->axes(QCPAxis::atTop | QCPAxis::atBottom)) {
        if (axis->selectedParts().testFlag(QCPAxis::spAxis) ||
                axis->selectedParts().testFlag(QCPAxis::spTickLabels))
            keyAxis = axis;
    }

    if (keyAxis == 0) {
        return;
    }

    keyAxis->setVisible(false);
    int dataSetId = keyAxis->property(AXIS_DATASET).toInt();

    emit axisVisibilityChangedForDataSet(dataSetId, false);
    ui->chart->replot();
}

void ChartWindow::changeSelectedKeyAxisTimespan() {
    dataset_id_t ds = getSelectedDataset();
    if (ds == INVALID_DATASET_ID)
        return;

    changeDataSetTimeSpan(ds);
}

void ChartWindow::changeDataSetTimeSpan(dataset_id_t dsId) {
    int index = -1;
    for (int i = 0; i < dataSets.count(); i++) {
        if (dataSets[i].id == dsId) {
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
        changeDataSetTimeSpan(dsId, dlg.getStartTime(), dlg.getEndTime());
    }
}

void ChartWindow::changeDataSetTimeSpan(dataset_id_t dsId, QDateTime start, QDateTime end) {
    int index = -1;
    for (int i = 0; i < dataSets.count(); i++) {
        if (dataSets[i].id == dsId) {
            index = i;
        }
    }
    if (index < 0) {
        return;
    }

    qDebug() << "Changing timespan for dataset" << dsId
             << "to:" << start << "-" << end;

    dataSets[index].startTime = start;
    dataSets[index].endTime = end;
    plotter->changeDataSetTimespan(dsId, start, end);
    emit dataSetTimeSpanChanged(dataSets[index].id, start, end);
}

void ChartWindow::setSelectedKeyAxisTickFormat() {
    dataset_id_t ds = getSelectedDataset();
    if (ds == INVALID_DATASET_ID)
        return;

    setSelectedKeyAxisTickFormat(ds);
}

void ChartWindow::setSelectedKeyAxisTickFormat(dataset_id_t dsId) {
    AxisTickFormatDialog atfd;

    key_axis_tick_format_t currentFormat = plotter->getKeyAxisTickFormat(dsId);
    QString currentFormatString = plotter->getKeyAxisTickFormatString(dsId);

    atfd.setFormat(currentFormat, currentFormatString);

    if (atfd.exec()) {
        key_axis_tick_format_t format = atfd.getFormat();
        QString formatString = atfd.getFormatString();
        plotter->setKeyAxisFormat(dsId, format, formatString);
    }
}

void ChartWindow::refreshSelectedKeyAxis() {
    dataset_id_t ds = getSelectedDataset();
    if (ds == INVALID_DATASET_ID)
        return;

    plotter->refreshDataSet(ds);
}

void ChartWindow::removeSelectedKeyAxis() {
    dataset_id_t ds = getSelectedDataset();
    if (ds == INVALID_DATASET_ID)
        return;

    removeDataSet(ds);
}

void ChartWindow::removeDataSet(dataset_id_t dsId) {

    if (hiddenDataSets.contains(dsId)) {
        hiddenDataSets.remove(dsId);
    }

    // Removing all graphs removes the dataset too and triggers a replot.
    SampleColumns cols;
    cols.standard = ALL_SAMPLE_COLUMNS;
    cols.extra = ALL_EXTRA_COLUMNS;
    plotter->removeGraphs(dsId, cols);

    int index = -1;
    for (int i = 0; i < dataSets.count(); i++) {
        if (dataSets[i].id == dsId) {
            index = i;
        }
    }
    if (index > -1) {
        dataSets.removeAt(index);
    }

    emit dataSetWasRemoved(dsId);
}

void ChartWindow::showValueAxisContextMenu(QPoint point, QCPAxis *axis) {

    // Deselect all Y axis
    foreach(QCPAxis* axis, ui->chart->axisRect()->axes(QCPAxis::atLeft | QCPAxis::atRight)) {
        axis->setSelectedParts(QCPAxis::spNone);
    }

    axis->setSelectedParts(QCPAxis::spAxis | QCPAxis::spTickLabels |
                           QCPAxis::spAxisLabel);
    ui->chart->replot();

    QMenu* menu = new QMenu(this);
    menu->setAttribute(Qt::WA_DeleteOnClose);

    menu->addAction(tr("&Rename"),
                    this, SLOT(renameSelectedValueAxis()));

    menu->addAction(tr("&Change Label Font..."), this, SLOT(changeAxisLabelFont()));
    menu->addAction(tr("&Change Tick Label Font..."), this, SLOT(changeAxisTickLabelFont()));

//    menu->addAction("&Move to Opposite",
//                    this, SLOT(moveSelectedValueAxisToOpposite()));

    menu->popup(ui->chart->mapToGlobal(point));
}



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


void ChartWindow::renameSelectedValueAxis() {
    QCPAxis* valueAxis = 0;

    foreach(QCPAxis* axis, ui->chart->axisRect()->axes(QCPAxis::atLeft | QCPAxis::atRight)) {
        if (axis->selectedParts().testFlag(QCPAxis::spAxis) ||
                axis->selectedParts().testFlag(QCPAxis::spTickLabels))
            valueAxis = axis;
    }

    if (valueAxis == 0) {
        return;
    }

    bool ok;
    QString name = QInputDialog::getText(
                this,
                tr("Rename Axis"),
                tr("New Axis Label:"),
                QLineEdit::Normal,
                valueAxis->label(),
                &ok);
    if (ok) {
        valueAxis->setLabel(name);
        emit dataSetRenamed(valueAxis->property(AXIS_DATASET).toInt(), name);
        ui->chart->replot();
    }
}

void ChartWindow::addTitle()
{
    bool ok;
    if (plotTitleValue.isNull()) {
        // Title has never been set. Ask for a value.
        plotTitleValue = QInputDialog::getText(
                    this,
                    tr("Chart Title"),
                    tr("New chart title:"),
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

void ChartWindow::toggleTitle() {
    if (plotTitleEnabled) {
        removeTitle();
    } else {
        addTitle();
    }
}

void ChartWindow::addTitle(QString title) {
    if (plotTitleEnabled) {
        removeTitle();
    }

    if (title.isEmpty()) {
        plotTitleEnabled = false;
        plotTitleValue = QString();
        setWindowTitle(tr("Untitled - Chart"));
        ui->chart->replot();
        return;
    }

    plotTitleEnabled = true;
    plotTitleValue = title;

    plotTitle = new QCPTextElement(ui->chart, title, plotTitleFont);

    connect(plotTitle, SIGNAL(doubleClicked(QMouseEvent*)), this, SLOT(textElementDoubleClick(QMouseEvent*)));

    plotTitle->setTextColor(plotTitleColour);
    plotTitle->setSelectable(true);

    ui->chart->plotLayout()->insertRow(0);
    ui->chart->plotLayout()->addElement(0, 0, plotTitle);

    if (!plotTitleValue.isEmpty()) {
        setWindowTitle(tr("%1 - Chart").arg(title));
    }

    ui->action_Title->setChecked(true);
}

void ChartWindow::removeTitle()
{
    plotTitleEnabled = false;
    ui->chart->plotLayout()->remove(plotTitle);
    ui->chart->plotLayout()->simplify();
    ui->action_Title->setChecked(false);

    ui->chart->replot(QCustomPlot::rpQueuedRefresh);
}

void ChartWindow::showLegendToggle()
{
    ui->chart->legend->setVisible(!ui->chart->legend->visible());
    ui->action_Legend->setChecked(ui->chart->legend->visible());
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
    ui->action_Grid->setChecked(gridVisible);

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

void ChartWindow::changeLegendFont() {
    QFont current = ui->chart->legend->font();
    bool ok;
    QFont newFont = QFontDialog::getFont(&ok, current, this, tr("Legend Font"));

    if (ok) {
        ui->chart->legend->setFont(newFont);
        ui->chart->replot();
    }
}

void ChartWindow::removeSelectedGraph()
{
    if (!ui->chart->selectedGraphs().isEmpty()) {
        QCPGraph* graph = ui->chart->selectedGraphs().first();

        dataset_id_t dataset = graph->property(GRAPH_DATASET).toInt();
        if (graph->property(COLUMN_TYPE).toString() == "standard") {
            StandardColumn column = (StandardColumn)graph->property(GRAPH_TYPE).toInt();

            // Turn off the column so it doesn't come back when the user
            // hits refresh.
            plotter->removeGraph(dataset, column);

            // Turn the column off in the dataset itself so it doesn't come back
            // later.
            for (int i = 0; i < dataSets.count(); i++) {
                if (dataSets[i].id == dataset) {
                    dataSets[i].columns.standard &= ~column;
                }
            }
        } else {
            ExtraColumn column = (ExtraColumn)graph->property(GRAPH_TYPE).toInt();

            // Turn off the column so it doesn't come back when the user
            // hits refresh.
            plotter->removeGraph(dataset, column);

            // Turn the column off in the dataset itself so it doesn't come back
            // later.
            for (int i = 0; i < dataSets.count(); i++) {
                if (dataSets[i].id == dataset) {
                    dataSets[i].columns.extra &= ~column;
                }
            }
        }

        // No more selected graph...
        setGraphActionsEnabled(false);
    }
}

void ChartWindow::renameSelectedGraph()
{
    if (!ui->chart->selectedGraphs().isEmpty()) {
        QCPGraph* graph = ui->chart->selectedGraphs().first();

        bool ok;
        QString title = QInputDialog::getText(
                    this,
                    tr("Rename Graph"),
                    tr("New graph name:"),
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


void ChartWindow::plottableDoubleClick(QCPAbstractPlottable* plottable, int dataIndex,
                                       QMouseEvent* event) {

    Q_UNUSED(dataIndex)
    Q_UNUSED(event)

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

dataset_id_t ChartWindow::getSelectedDataset() {
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
        return INVALID_DATASET_ID;
    }

    dataset = keyAxis->property(AXIS_DATASET).toInt();

#endif

    return dataset;
}

void ChartWindow::addGraph()
{
    dataset_id_t ds = getSelectedDataset();
    if (ds == INVALID_DATASET_ID)
        return;

    showAddGraph(ds);
}

void ChartWindow::showAddGraph(dataset_id_t dsId) {

    AddGraphDialog adg(plotter->availableColumns(dsId),
                       solarDataAvailable,
                       isWireless,
                       hw_type,
                       extraColumns,
                       extraColumnNames,
                       this);
    if (adg.exec() == QDialog::Accepted) {
        plotter->addGraphs(dsId, adg.selectedColumns());
    }
}

void ChartWindow::copy() {
    QApplication::clipboard()->setPixmap(ui->chart->toPixmap());
}

void ChartWindow::save() {

    QString pdfFilter = tr("Adobe Portable Document Format (*.pdf)");
    QString pngFilter = tr("Portable Network Graphics (*.png)");
    QString jpgFilter = tr("JPEG (*.jpg)");
    QString bmpFilter = tr("Windows Bitmap (*.bmp)");
    QString svgFilter = tr("Scalable Vector Graphics (*.svg)");

    QString filter = pngFilter + ";;" + svgFilter + ";;" + pdfFilter + ";;" +
            jpgFilter + ";;" + bmpFilter;

    QString selectedFilter;

    QString fileName = QFileDialog::getSaveFileName(
                this, tr("Save As"), "", filter, &selectedFilter);


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
    else if (selectedFilter == svgFilter) {
        QSvgGenerator generator;
        generator.setFileName(fileName);
        if (plotTitleEnabled) {
            generator.setTitle(plotTitleValue);
        } else {
            generator.setTitle(tr("Plot"));
        }
        generator.setDescription(tr("Generated by zxweather"));
        QCPPainter qcpPainter;
        qcpPainter.begin(&generator);
        ui->chart->toPainter(&qcpPainter, ui->chart->width(), ui->chart->height());
        qcpPainter.end();
    }
}

void ChartWindow::addDataSet() {
    ChartOptionsDialog options(solarDataAvailable, hw_type, isWireless,
                               extraColumns,
                               extraColumnNames);
    options.setWindowTitle(tr("Add Data Set"));
    options.setWindowIcon(QIcon(":/icons/dataset-add"));

    int result = options.exec();
    if (result != QDialog::Accepted)
        return; // User canceled. Nothing to do.

    SampleColumns columns = options.getColumns();

    DataSet ds;
    ds.columns = columns;
    ds.extraColumnNames = extraColumnNames;
    ds.startTime = options.getStartTime();
    ds.endTime = options.getEndTime();
    ds.aggregateFunction = options.getAggregateFunction();
    ds.groupType = options.getAggregateGroupType();
    ds.customGroupMinutes = options.getCustomMinutes();
    ds.id = nextDataSetId;
    dataSets.append(ds);
    nextDataSetId++;

    emit dataSetAdded(ds, QString());

    // This will resuilt in the chart being redrawn completely.
    // TODO: don't rebuild the entire chart. Just add the new data set.
    plotter->addDataSet(ds);
    reloadDataSets(false);
}

void ChartWindow::setDataSetAxisVisibility(dataset_id_t dsId, bool visible) {
    foreach (QCPAxis* axis, ui->chart->axisRect()->axes(QCPAxis::atBottom | QCPAxis::atTop)) {
        dataset_id_t id = axis->property(AXIS_DATASET).toInt();
        if (dsId == id) {
            axis->setVisible(visible);
            emit axisVisibilityChangedForDataSet(dsId, visible);
            ui->chart->replot();
            return;
        }
    }
}

void ChartWindow::setDataSetName(dataset_id_t dsId, QString name) {
    foreach (QCPAxis* axis, ui->chart->axisRect()->axes(QCPAxis::atBottom | QCPAxis::atTop)) {
        dataset_id_t id = axis->property(AXIS_DATASET).toInt();
        if (dsId == id) {
            axis->setLabel(name);
            emit dataSetRenamed(dsId, name);
            ui->chart->replot();
            return;
        }
    }
}

void ChartWindow::selectDataSet(dataset_id_t dsId) {
    qDebug() << "Select dataset" << dsId;

    // Select its axis
    foreach (QCPAxis *axis, ui->chart->axisRect()->axes(QCPAxis::atBottom | QCPAxis::atTop)) {
        dataset_id_t id = axis->property(AXIS_DATASET).toInt();
        if (id == dsId) {
            axis->setSelectedParts(QCPAxis::spAxis | QCPAxis::spTickLabels | QCPAxis::spAxisLabel);
        } else {
            axis->setSelectedParts(QCPAxis::spNone);
        }
    }

    foreach (QCPGraph* g, ui->chart->axisRect()->graphs()) {
        dataset_id_t id = g->property(GRAPH_DATASET).toInt();
        if (id == dsId) {
            g->setSelection(QCPDataSelection(QCPDataRange(0, 1)));
            QCPPlottableLegendItem *lip = ui->chart->legend->itemWithPlottable(g);
            lip->setSelected(true);
        } else {
            g->setSelection(QCPDataSelection(QCPDataRange(0, 0)));
            QCPPlottableLegendItem *lip = ui->chart->legend->itemWithPlottable(g);
            lip->setSelected(false);
        }
    }
    ui->chart->replot();
}

void ChartWindow::setDataSetVisibility(dataset_id_t dsId, bool visible) {
    qDebug() << "Select dataset" << dsId;

    // Select its axis
    foreach (QCPAxis *axis, ui->chart->axisRect()->axes(QCPAxis::atBottom | QCPAxis::atTop)) {
        dataset_id_t id = axis->property(AXIS_DATASET).toInt();
        if (id == dsId) {
            axis->setVisible(visible);
            break;
        }
    }

    foreach (QCPGraph* g, ui->chart->axisRect()->graphs()) {
        dataset_id_t id = g->property(GRAPH_DATASET).toInt();
        if (id == dsId) {
            g->setVisible(visible);
            g->setSelection(QCPDataSelection(QCPDataRange(0, 1)));

            if (visible) {
                g->addToLegend();
            } else {
                g->removeFromLegend();
            }
        }
    }

    if (visible && hiddenDataSets.contains(dsId)) {
        hiddenDataSets.remove(dsId);
    } else if (!visible) {
        hiddenDataSets.insert(dsId);
    }

    emit axisVisibilityChangedForDataSet(dsId, visible);
    emit dataSetVisibilityChanged(dsId, visible);

    ui->chart->replot();
}

void ChartWindow::showDataSetsWindow() {
    if (!dds.isNull()) {
        dds->show();
        dds->activateWindow();
        return;
    }

    QMap<dataset_id_t, QString> names;
    QMap<dataset_id_t, bool> axisVisibility;
    QMap<dataset_id_t, bool> visibility;

    foreach (DataSet d, dataSets) {
        foreach (QCPAxis* axis, ui->chart->axisRect()->axes(QCPAxis::atBottom | QCPAxis::atTop)) {
            int dsId = axis->property(AXIS_DATASET).toInt();
            if (dsId == d.id) {
                names[d.id] = axis->label();
                axisVisibility[d.id] = axis->visible();
            }
        }
    }

    foreach (dataset_id_t id, hiddenDataSets) {
        visibility[id] = false;
    }

    dds.reset(new DataSetsDialog(dataSets, names, axisVisibility, visibility));

    connect(dds.data(), SIGNAL(addDataSet()), this, SLOT(addDataSet()));
    connect(this, SIGNAL(axisVisibilityChangedForDataSet(dataset_id_t,bool)),
            dds.data(), SLOT(axisVisibilityChangedForDataSet(dataset_id_t,bool)));
    connect(dds.data(), SIGNAL(axisVisibilityChanged(dataset_id_t,bool)),
            this, SLOT(setDataSetAxisVisibility(dataset_id_t,bool)));

    connect(dds.data(), SIGNAL(dataSetSelected(dataset_id_t)),
            this, SLOT(selectDataSet(dataset_id_t)));

    connect(dds.data(), SIGNAL(dataSetVisibilityChanged(dataset_id_t,bool)),
            this, SLOT(setDataSetVisibility(dataset_id_t,bool)));
    connect(this, SIGNAL(dataSetVisibilityChanged(dataset_id_t,bool)),
            dds.data(), SLOT(visibilityChangedForDataSet(dataset_id_t,bool)));

    connect(this, SIGNAL(dataSetAdded(DataSet, QString)),
            dds.data(), SLOT(dataSetAdded(DataSet, QString)));
    connect(this, SIGNAL(dataSetWasRemoved(dataset_id_t)),
            dds.data(), SLOT(dataSetRemoved(dataset_id_t)));

    connect(this, SIGNAL(dataSetRenamed(dataset_id_t,QString)),
            dds.data(), SLOT(dataSetRenamed(dataset_id_t,QString)));
    connect(dds.data(), SIGNAL(dataSetNameChanged(dataset_id_t,QString)),
            this, SLOT(setDataSetName(dataset_id_t,QString)));

    connect(this, SIGNAL(dataSetTimeSpanChanged(dataset_id_t,QDateTime,QDateTime)),
            dds.data(), SLOT(dataSetTimeSpanChanged(dataset_id_t,QDateTime,QDateTime)));
    connect(dds.data(), SIGNAL(changeTimeSpan(dataset_id_t)),
            this, SLOT(changeDataSetTimeSpan(dataset_id_t)));

    connect(dds.data(), SIGNAL(addGraph(dataset_id_t)),
            this, SLOT(showAddGraph(dataset_id_t)));

    connect(dds.data(), SIGNAL(removeDataSet(dataset_id_t)),
            this, SLOT(removeDataSet(dataset_id_t)));

    dds->show();
}
