#include "liveplot.h"

#include <QApplication>
#include <QFileDialog>
#include <QClipboard>
#include <QLineEdit>
#include <QInputDialog>
#include <QAction>
#include <QMenu>

#include "graphstyledialog.h"
#include "settings.h"

#define PROP_GRAPH_ID "GraphId"

LivePlot::LivePlot(QWidget *parent) : QCustomPlot(parent)
{
    nextId = 0;

    // QCP::iRangeDrag | QCP::iRangeZoom |
    setInteractions(QCP::iSelectAxes | QCP::iSelectLegend | QCP::iSelectPlottables);
    setContextMenuPolicy(Qt::CustomContextMenu);

    connect(this, SIGNAL(plottableClick(QCPAbstractPlottable*,int,QMouseEvent*)),
            this, SLOT(plottableClicked(QCPAbstractPlottable*,int,QMouseEvent*)));
    connect(this, SIGNAL(plottableDoubleClick(QCPAbstractPlottable*,int,QMouseEvent*)),
            this, SLOT(plottableDoubleClicked(QCPAbstractPlottable*,int,QMouseEvent*)));
    connect(this, SIGNAL(legendClick(QCPLegend*,QCPAbstractLegendItem*,QMouseEvent*)),
            this, SLOT(legendClicked(QCPLegend*,QCPAbstractLegendItem*,QMouseEvent*)));
    connect(this, SIGNAL(legendDoubleClick(QCPLegend*,QCPAbstractLegendItem*,QMouseEvent*)),
            this, SLOT(legendDoubleClicked(QCPLegend*,QCPAbstractLegendItem*,QMouseEvent*)));
    connect(this, SIGNAL(customContextMenuRequested(QPoint)),
            this, SLOT(chartContextMenuRequested(QPoint)));
    connect(this, SIGNAL(axisDoubleClick(QCPAxis*,QCPAxis::SelectablePart,QMouseEvent*)),
            this, SLOT(axisDoubleClicked(QCPAxis*,QCPAxis::SelectablePart,QMouseEvent*)));
}

void LivePlot::recreateDefaultAxisRect() {
    // Recreate the default axis rect and legend as done in the QCustomPlot
    // constructor. First we'll clear everything else out.

    if (graphCount() > 0) {
        removeAllGraphs();
    }

    QCPAxisRect *defaultAxisRect = new QCPAxisRect(this, true);
    mPlotLayout->addElement(0, 0, defaultAxisRect);
    xAxis = defaultAxisRect->axis(QCPAxis::atBottom);
    yAxis = defaultAxisRect->axis(QCPAxis::atLeft);
    xAxis2 = defaultAxisRect->axis(QCPAxis::atTop);
    yAxis2 = defaultAxisRect->axis(QCPAxis::atRight);
    legend = new QCPLegend;
    legend->setVisible(false);
    defaultAxisRect->insetLayout()->addElement(legend, Qt::AlignRight|Qt::AlignTop);
    defaultAxisRect->insetLayout()->setMargins(QMargins(12, 12, 12, 12));

    defaultAxisRect->setLayer(QLatin1String("background"));
    xAxis->setLayer(QLatin1String("axes"));
    yAxis->setLayer(QLatin1String("axes"));
    xAxis2->setLayer(QLatin1String("axes"));
    yAxis2->setLayer(QLatin1String("axes"));
    xAxis->grid()->setLayer(QLatin1String("grid"));
    yAxis->grid()->setLayer(QLatin1String("grid"));
    xAxis2->grid()->setLayer(QLatin1String("grid"));
    yAxis2->grid()->setLayer(QLatin1String("grid"));
    legend->setLayer(QLatin1String("legend"));
}

void LivePlot::copy() {
    QApplication::clipboard()->setPixmap(toPixmap());
}

void LivePlot::save() {
    QString pdfFilter = "Adobe Portable Document Format (*.pdf)";
    QString pngFilter = "Portable Network Graphics (*.png)";
    QString jpgFilter = "JPEG (*.jpg)";
    QString bmpFilter = "Windows Bitmap (*.bmp)";

    QString filter = pngFilter + ";;" + pdfFilter + ";;" +
            jpgFilter + ";;" + bmpFilter;

    QString selectedFilter;

    QString fileName = QFileDialog::getSaveFileName(
                this, "Save As" ,"", filter, &selectedFilter);


    // To prevent selected stuff appearing in the output
    deselectAll();

    // TODO: Provide a UI to ask for width, height, cosmetic pen (PS), etc.
    if (selectedFilter == pdfFilter)
        savePdf(fileName);
    else if (selectedFilter == pngFilter)
        savePng(fileName);
    else if (selectedFilter == jpgFilter)
        saveJpg(fileName);
    else if (selectedFilter == bmpFilter)
        saveBmp(fileName);
}

QCPGraph* LivePlot::addStyledGraph(QCPAxis *keyAxis, QCPAxis *valueAxis, GraphStyle style) {
    QCPGraph* graph = new QCPGraph(keyAxis, valueAxis);
    graph->setProperty(PROP_GRAPH_ID, nextId);
    graphStyles[nextId] = style;
    nextId++;

    graph->setSelectable(QCP::stWhole);

    return graph;
}

void LivePlot::changeSelectedGraphStyle()
{
    if (!selectedGraphs().isEmpty()) {
        QCPGraph* graph = selectedGraphs().first();
        changeGraphStyle(graph);
    }
}

void LivePlot::changeGraphStyle(QCPGraph* graph) {
    if (graph == NULL) {
        qWarning() << "NULL graph while attempting to change style";
        return;
    }

    QVariant graphId = graph->property(PROP_GRAPH_ID);

    if (!graphId.isValid() || graphId.isNull() || graphId.type() != QVariant::Int) {
        return; // We don't have a valid graph Id so no style information to edit.
    }

    GraphStyle& style = graphStyles[graph->property(PROP_GRAPH_ID).toInt()];

    // The Graph Style Dialog updates the style directly on accept.
    GraphStyleDialog gsd(style, this);
    int result = gsd.exec();
    if (result == QDialog::Accepted) {
        style.applyStyle(graph);
        emit graphStyleChanged(graph, style);
        replot();
    }
}


void LivePlot::plottableClicked(QCPAbstractPlottable* plottableItem,
                                int dataIndex,
                                QMouseEvent* event) {
    Q_UNUSED(dataIndex);
    Q_UNUSED(event);

    if (plottableItem->selected() && legend != NULL) {

        // Clear selected items.
        for (int i = 0; i < legend->itemCount(); i++) {
            QCPAbstractLegendItem* item = legend->item(i);
            item->setSelected(false);
        }

        QCPPlottableLegendItem *lip = legend->itemWithPlottable(plottableItem);
        if (lip != NULL)  {
            lip->setSelected(true);
        }
    }
}

void LivePlot::plottableDoubleClicked(QCPAbstractPlottable* plottable,
                                      int /*dataIndex*/,
                                      QMouseEvent* /*event*/) {

    QCPGraph *graph = qobject_cast<QCPGraph *>(plottable);

    if (graph == 0) {
        // Its not a QCPGraph. Whatever it is we don't currently support
        // customising its style
    }

    changeGraphStyle(graph);
}

void LivePlot::legendClicked(QCPLegend* /*legend*/,
                            QCPAbstractLegendItem *item,
                            QMouseEvent* /*event*/) {

    /*
    * Select the plottable associated with a legend item when the legend
    * item is selected.
    */

    QCPPlottableLegendItem* plotItem = qobject_cast<QCPPlottableLegendItem*>(item);

    if (plotItem == NULL) {
        qDebug() << "Not a plottable legend item.";
        // The legend item isn't for a plottable. nothing to do here.
        return;
    }

    QCPAbstractPlottable* plottableItem = plotItem->plottable();

    // Deselect any other selected plottables.
    for (int i = 0; i < plottableCount(); i++) {
        // This will deselect everything.
        this->plottable(i)->setSelection(QCPDataSelection(QCPDataRange(0, 0)));
    }

    // Then select the plottable associated with this legend item.
    if (plotItem->selected()) {
        // Any arbitrary selection range will select the whole lot plottable
        // when the selection mode is Whole
        plottableItem->setSelection(QCPDataSelection(QCPDataRange(0,1)));
        emit selectionChangedByUser();
    }
}

void LivePlot::legendDoubleClicked(QCPLegend* /*legend*/,
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

void LivePlot::addTitle()
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
        replot();
    }
}

void LivePlot::addTitle(QString title) {
    if (plotTitleEnabled) {
        removeTitle(false);
    }
    plotTitleEnabled = true;
    plotTitleValue = title;

    plotTitle = new QCPTextElement(this, title, QFont("sans", 12, QFont::Bold));

    connect(plotTitle, SIGNAL(doubleClicked(QMouseEvent*)),
            this, SLOT(textElementDoubleClick(QMouseEvent*)));

    plotTitle->setTextColor(Settings::getInstance().getChartColours().title);
    plotLayout()->insertRow(0);
    plotLayout()->addElement(0, 0, plotTitle);
}

void LivePlot::removeTitle(bool replot)
{
    plotTitleEnabled = false;
    plotLayout()->remove(plotTitle);
    plotLayout()->simplify();



    if (replot) {
        this->replot();
    }
}

void LivePlot::textElementDoubleClick(QMouseEvent *event)
{
    Q_UNUSED(event)
    if (QCPTextElement *element = qobject_cast<QCPTextElement*>(sender())) {
        bool ok;
        QString newTitle = QInputDialog::getText(
                    this,
                    tr("Change Text"),
                    tr("Change text:"),
                    QLineEdit::Normal,
                    element->text(),
                    &ok);

        if (ok) {
            element->setText(newTitle);
            replot();
        }
    }
}

void LivePlot::toggleLegend()
{
    if (legend == NULL) return;

    legend->setVisible(!legend->visible());
    emit legendVisibilityChanged(legend->visible());
    replot();
}

void LivePlot::toggleTitle()
{
    if (plotTitle.isNull())
        addTitle();
    else
        removeTitle(true);

    emit titleVisibilityChanged(plotTitleEnabled);
}

void LivePlot::moveLegend()
{
    if (QAction* menuAction = qobject_cast<QAction*>(sender())) {
        // We were called by a context menu. We should have the necessary
        // data.
        bool ok;
        int intData = menuAction->data().toInt(&ok);
        if (ok) {
            axisRect()->insetLayout()->setInsetAlignment(
                        0, (Qt::Alignment)intData);
            replot();
        }
    }
}

void LivePlot::showLegendContextMenu(QPoint point)
{
    QMenu *menu = new QMenu(this);
    menu->setAttribute(Qt::WA_DeleteOnClose);

    for (int i = 0; i < legend->itemCount(); i++) {
        if (legend->item(i)->selectTest(point, false) >= 0) {
           QCPAbstractLegendItem *item = legend->item(i);
           QCPPlottableLegendItem *plottableItem = qobject_cast<QCPPlottableLegendItem*>(item);
           if (plottableItem != NULL) {
               // Deselect any currently selected plottables
               for (int i = 0; i < plottableCount(); i++) {
                   plottable(i)->setSelection(QCPDataSelection(QCPDataRange(0, 0)));
               }

               // select the graph
               plottableItem->setSelected(true);
               plottableItem->plottable()->setSelection(QCPDataSelection(QCPDataRange(0,1)));

               replot(QCustomPlot::rpRefreshHint);

               // And add on the graphs context menu options
               menu->addAction("Remove graph", this, SLOT(removeSelectedGraph()));
               menu->addSeparator();

               // We've found the legend item that was right-clicked - no need to search
               // any further.
               break;
           }
        }
    }

    bool inRect = false;

    // Figure out if the legend is currently inside the default axis
    // rect. If so we'll give some options to reposition it within that
    // rect.
    if (axisRectCount() > 0) {
        if (axisRect()->insetLayout()->children().contains(legend)) {
            inRect = true;
        }
    }

    if (inRect) {
        // Options to re-position the legend within an axis rect
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


        menu->addSeparator();
    }

    // And an option to get rid of it entirely.
    menu->addAction("Hide", this, SLOT(toggleLegend()));

    menu->popup(mapToGlobal(point));
}

void LivePlot::chartContextMenuRequested(QPoint point)
{
    // Check to see if the legend was right-clicked on
    if (legend != NULL && legend->selectTest(point, false) >= 0
            && legend->visible()) {
        showLegendContextMenu(point);
        return;
    }

    showChartContextMenu(point);
}

void LivePlot::renameSelectedGraph()
{
    if (!selectedGraphs().isEmpty()) {
        QCPGraph* graph = selectedGraphs().first();

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

            QVariant graphId = graph->property(PROP_GRAPH_ID);

            if (!graphId.isValid() || graphId.isNull() || graphId.type() != QVariant::Int) {
                 // We don't have a valid graph Id so no style information to edit.
            } else {
                graphStyles[graphId.toInt()].setName(title);
            }

            replot();
        }
    }
}

void LivePlot::removeSelectedGraph()
{
    if (!selectedGraphs().isEmpty()) {
        QCPGraph* graph = selectedGraphs().first();

        emit removingGraph(graph);
        removeGraph(graph);

        // Prune away any unused value axes
        foreach (QCPAxisRect *rect, axisRects()) {
            if (rect->graphs().count() == 0) {
                qDebug() << "Axis rect now empty. Removing.";

                // Rect is empty. Trash the whole thing.
                plotLayout()->remove(rect);
                plotLayout()->simplify();
            } else {
                QList<QCPAxis*> axes = rect->axes(QCPAxis::atLeft | QCPAxis::atRight);
                for (int i = 0; i < axes.count(); i++) {
                    QCPAxis *axis = axes.at(i);
                    if (axis->graphs().isEmpty()) {
                        qDebug() << "Axis " << axis->label() << "has no graphs - removing.";
                        rect->removeAxis(axis);
                    }
                }
            }
        }

        // User removed the selected graph. Graph no longer selected.
        emit selectionChangedByUser();

        replot();
    }
}

void LivePlot::removeAllGraphs() {
    qDebug() << "Remove all graphs...";
    foreach (QCPAxisRect *rect, axisRects()) {
        QList<QCPGraph*> graphs = rect->graphs();
        foreach (QCPGraph* graph, graphs) {
            qDebug() << "Selecting Graph" << graph->name();
            graph->setSelection(QCPDataSelection(QCPDataRange(0,1)));
            removeSelectedGraph();
        }
    }
}

void LivePlot::showChartContextMenu(QPoint point) {
    QMenu* menu = new QMenu(this);
    menu->setAttribute(Qt::WA_DeleteOnClose);

    QAction *action;

    /******** Graph remove ********/
    // If a graph is currently selected let it be removed.
    if (!selectedGraphs().isEmpty()) {
        menu->addAction("Remove selected graph",
                        this, SLOT(removeSelectedGraph()));
    }

    menu->addSeparator();
    menu->addAction(QIcon(":/icons/save"), "&Save...", this, SLOT(save()));
    menu->addAction("&Copy", this, SLOT(copy()));

    /******** Graph add ********/
    action = menu->addAction(QIcon(":/icons/chart-add"), "Add Graph...",
                                      this, SLOT(emitAddGraphRequested()));
    action->setEnabled(addGraphsEnabled);

    /******** Plot feature visibility & layout ********/
    menu->addSeparator();

    // Title visibility option.
    action = menu->addAction("Show Title",
                             this, SLOT(toggleTitle()));
    action->setCheckable(true);
    action->setChecked(!plotTitle.isNull());


    // Legend visibility option.
    if (legend != NULL) {
        action = menu->addAction("Show Legend",
                                 this, SLOT(toggleLegend()));
        action->setCheckable(true);
        action->setChecked(legend->visible());
    }


    /******** Finished ********/
    menu->popup(mapToGlobal(point));
}

void LivePlot::emitAddGraphRequested() {
    emit addGraphRequested();
}

void LivePlot::setAddGraphsEnabled(bool enabled) {
    this->addGraphsEnabled = enabled;
}

void LivePlot::axisDoubleClicked(QCPAxis *axis, QCPAxis::SelectablePart part,
                                  QMouseEvent *event)
{
    Q_UNUSED(event)

    // If the user double-clicked on the axis label then ask for new
    // label text.
    if (part == QCPAxis::spAxisLabel) {
        bool ok;

        QString newLabel = QInputDialog::getText(
                    this,
                    "Axis Label",
                    "New axis label:",
                    QLineEdit::Normal,
                    axis->label(),
                    &ok);
        if (ok) {
            axis->setLabel(newLabel);
            replot();
        }
    }
}
