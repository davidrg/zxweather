#include "rainfallwidget.h"

#include <QLabel>
#include <QFrame>
#include <QGridLayout>
#include <QTemporaryFile>
#include <QApplication>
#include <QUrl>
#include <QDesktopServices>
#include <QDir>
#include <QMenu>
#include <QAction>
#include <QFileDialog>
#include <QClipboard>

#include "charts/qcp/qcustomplot.h"
#include "unit_conversions.h"
#include "settings.h"


#define K_DAY 1
#define K_STORM 2
#define K_RATE 3
#define K_MONTH 1
#define K_YEAR 2

RainfallWidget::RainfallWidget(QWidget *parent) : QWidget(parent)
{
    connect(&Settings::getInstance(), SIGNAL(unitsChanged(bool,bool)),
            this, SLOT(unitsChanged(bool,bool)));
    imperial = Settings::getInstance().imperial();


    // Create the basic UI
    plot = new QCustomPlot(this);
    plot->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Expanding);

    connect(plot, SIGNAL(mousePress(QMouseEvent*)),
            this, SLOT(mousePressEventSlot(QMouseEvent*)));
    connect(plot, SIGNAL(mouseMove(QMouseEvent*)),
            this, SLOT(mouseMoveEventSlot(QMouseEvent*)));
    connect(plot, SIGNAL(plottableDoubleClick(QCPAbstractPlottable*,int,QMouseEvent*)),
            this, SLOT(plottableDoubleClick(QCPAbstractPlottable*,int,QMouseEvent*)));
    connect(plot, SIGNAL(customContextMenuRequested(QPoint)),
            this, SLOT(showContextMenu(QPoint)));

    plot->setContextMenuPolicy(Qt::CustomContextMenu);

    QFrame *plotFrame = new QFrame(this);
    plotFrame->setFrameShape(QFrame::StyledPanel);
    plotFrame->setFrameShadow(QFrame::Plain);

    QGridLayout *fL = new QGridLayout();
    fL->addWidget(plot, 0, 0);
    fL->setMargin(0);
    plotFrame->setLayout(fL);

    label = new QLabel("<b>Rainfall</b>", this);
    line = new QFrame(this);
    line->setFrameShape(QFrame::HLine);
    line->setFrameShadow(QFrame::Sunken);
    stormRateEnabled = true;

    QGridLayout *l = new QGridLayout();
    l->addWidget(label, 0, 0);
    l->addWidget(line, 1, 0);
    l->addWidget(plotFrame, 2, 0);

    l->setMargin(0);

    setLayout(l);

    // Configure the Plot widget
    plot->plotLayout()->clear(); // Throw away default axis rect

    // First the axis rect for day/storm/rate
    QCPAxisRect *smallRect = new QCPAxisRect(plot);
    smallRect->setupFullAxesBox(true);
    smallRect->axis(QCPAxis::atTop)->setVisible(true);
    smallRect->axis(QCPAxis::atRight)->setTickLabels(true);
    smallRect->axis(QCPAxis::atBottom)->grid()->setVisible(false);
    smallRect->axis(QCPAxis::atLeft)->grid()->setVisible(false);
    plot->plotLayout()->addElement(0,0, smallRect);

    // Configure ticks
    QSharedPointer<QCPAxisTickerText> shortRangeTopTicker(new QCPAxisTickerText());
    shortRangeTopTicker->addTick(K_DAY, tr("Day"));
    shortRangeTopTicker->addTick(K_STORM, tr("Storm"));
    shortRangeTopTicker->addTick(K_RATE, tr("Rate"));
    smallRect->axis(QCPAxis::atTop)->setTicker(shortRangeTopTicker);
    smallRect->axis(QCPAxis::atTop)->setTickLabels(true);

    shortRangeBottomTicker = QSharedPointer<QCPAxisTickerText>(new QCPAxisTickerText());
    shortRangeBottomTicker->addTick(K_DAY, "0");
    shortRangeBottomTicker->addTick(K_STORM, "0");
    shortRangeBottomTicker->addTick(K_RATE, "0");
    smallRect->axis(QCPAxis::atBottom)->setTicker(shortRangeBottomTicker);

    smallRect->axis(QCPAxis::atLeft)->setTickLength(0,4);
    smallRect->axis(QCPAxis::atLeft)->setSubTickLength(0,2);
    smallRect->axis(QCPAxis::atRight)->setTickLength(0,4);
    smallRect->axis(QCPAxis::atRight)->setSubTickLength(0,2);

    // Then the larger-ranged axis for month/year
    QCPAxisRect *largeRect = new QCPAxisRect(plot);
    largeRect->setupFullAxesBox(true);
    largeRect->axis(QCPAxis::atTop)->setVisible(true);
    largeRect->axis(QCPAxis::atRight)->setTickLabels(true);
    largeRect->axis(QCPAxis::atBottom)->grid()->setVisible(false);
    largeRect->axis(QCPAxis::atLeft)->grid()->setVisible(false);
    plot->plotLayout()->addElement(0,1, largeRect);

    // Configure ticks   
    QSharedPointer<QCPAxisTickerText> longRangeTopTicker(new QCPAxisTickerText());
    longRangeTopTicker->addTick(K_MONTH, tr("Month"));
    longRangeTopTicker->addTick(K_YEAR, tr("Year"));
    largeRect->axis(QCPAxis::atTop)->setTicker(longRangeTopTicker);
    largeRect->axis(QCPAxis::atTop)->setTickLabels(true);

    longRangeBottomTicker = QSharedPointer<QCPAxisTickerText>(new QCPAxisTickerText());
    longRangeBottomTicker->addTick(K_MONTH, "0");
    longRangeBottomTicker->addTick(K_YEAR, "0");
    largeRect->axis(QCPAxis::atBottom)->setTicker(longRangeBottomTicker);

    largeRect->axis(QCPAxis::atLeft)->setTickLength(0,4);
    largeRect->axis(QCPAxis::atLeft)->setSubTickLength(0,2);
    largeRect->axis(QCPAxis::atRight)->setTickLength(0,4);
    largeRect->axis(QCPAxis::atRight)->setSubTickLength(0,2);

    // Now the bars. First again, the day/storm/rate bars
    shortRange = new QCPBars(smallRect->axis(QCPAxis::atBottom),
                             smallRect->axis(QCPAxis::atLeft));
    shortRange->setBrush(QBrush(Qt::blue));
    shortRange->setWidthType(QCPBars::wtAxisRectRatio);
    shortRange->setWidth(1.0/3.3); // 3 bars + padding
    shortRange->keyAxis()->setRange(0.5,3.5);
    shortRange->valueAxis()->setRange(0, 5.0);


    // Month/year
    longRange = new QCPBars(largeRect->axis(QCPAxis::atBottom),
                            largeRect->axis(QCPAxis::atLeft));
    longRange->setBrush(QBrush(Qt::blue));
    longRange->setWidthType(QCPBars::wtAxisRectRatio);
    longRange->setWidth(1.0/2.15); // 2 bars + padding
    longRange->keyAxis()->setRange(0.5,2.5);
    longRange->valueAxis()->setRange(0, 5.0);


    setRain(QDate::currentDate(), 0, 0, 0); // initalise variables

//  These seem to have been observed setting the shortRage key axis
//  range to weird values. Commenting them out appeared to make the
//  bug go away with no visible side-effects. Who knows why we were
//  even rescaling the key axes given we're setting the range on them
//  manually.
//    shortRange->rescaleKeyAxis();
//    longRange->rescaleKeyAxis();

    plot->replot();

    // Create a temporary filename for use in drag&drop operations
#if QT_VERSION >= 0x050000
        QString filename = QStandardPaths::writableLocation(
                    QStandardPaths::CacheLocation) + QDir::separator() + "temp";
#else
        QString filename = QDesktopServices::storageLocation(
                    QDesktopServices::TempLocation);
#endif

    if (!QDir(filename).exists())
        QDir().mkpath(filename);

    QTemporaryFile f(filename + QDir::separator() + "XXXXXX.png");
    f.setAutoRemove(false);
    f.open();
    tempFileName = f.fileName();

    reset(); // initialise state variables
}

RainfallWidget::~RainfallWidget() {
    QFile(tempFileName).remove();
}

void RainfallWidget::unitsChanged(bool imperial, bool kmh) {
    Q_UNUSED(kmh);

    this->imperial = imperial;
    reset();
}

void RainfallWidget::mousePressEvent(QMouseEvent *event)
{
    if (event == NULL) {
        return;
    }

    if (event->button() == Qt::LeftButton) {
        dragStartPos = event->pos();
    }

    QWidget::mousePressEvent(event);
}

void RainfallWidget::mouseMoveEvent(QMouseEvent *event) {
    if (event->buttons() & Qt::LeftButton) {
        int distance = (event->pos() - dragStartPos).manhattanLength();
        if (distance >= QApplication::startDragDistance()) {
            startDrag();
        }
    }

    QWidget::mouseMoveEvent(event);
}

void RainfallWidget::mousePressEventSlot(QMouseEvent *event) {
    mousePressEvent(event);
}

void RainfallWidget::mouseMoveEventSlot(QMouseEvent *event) {
    mouseMoveEvent(event);
}

void RainfallWidget::startDrag() {
    qDebug() << "Start drag";
    QPixmap pix = plot->toPixmap();

    pix.save(tempFileName);
    qDebug() << tempFileName;

    QList<QUrl> urls;
    urls << QUrl::fromLocalFile(tempFileName);

    QMimeData *mimeData = new QMimeData;
    mimeData->setUrls(urls);

    QDrag *drag = new QDrag(this);
    drag->setMimeData(mimeData);

    drag->exec(Qt::CopyAction, Qt::CopyAction);
}

void RainfallWidget::reset() {
    setStormRateEnabled(true);
    day = 0;
    storm = 0;
    rate = 0;
    month = 0;
    year = 0;
    rainExtra = 0;
    lastStormRain = -1;
    lastUpdate = QDate::currentDate();

    if (Settings::getInstance().imperial()) {
        shortRange->valueAxis()->setRange(0, 0.5);
        longRange->valueAxis()->setRange(0, 5);
    } else {
        shortRange->valueAxis()->setRange(0, 10);
        longRange->valueAxis()->setRange(0, 100);
    }

    updatePlot();
}

void RainfallWidget::setStormRateEnabled(bool enabled) {
    if (enabled == stormRateEnabled) {
        return; // no change.
    }

    stormRateEnabled = enabled;

    /*QVector<QString> smallTickTopLabels;
    smallTickTopLabels << "Day";

    if (enabled) {
        smallTickTopLabels  << "Storm" << "Rate";
    }*/

    //shortRange->valueAxis()->axisRect()->axis(QCPAxis::atTop)->setTickVectorLabels(smallTickTopLabels);

    if (enabled) {
        shortRange->keyAxis()->setRange(0.5,3.5);
    } else {
        shortRange->keyAxis()->setRange(0.5,1.5);
    }

    updatePlot();
}

void RainfallWidget::liveData(LiveDataSet lds) {
    if (stormRateEnabled) {
        storm = lds.davisHw.stormRain;
        rate = lds.davisHw.rainRate;

        stormStart = lds.davisHw.stormStartDate;
        stormValid = lds.davisHw.stormDateValid;

        // Compute rainfall since last sample from changes in storm rain
        if (lastStormRain > -1 && lds.davisHw.stormRain > lastStormRain) {
            rainExtra += lds.davisHw.stormRain - lastStormRain;
        }

        lastStormRain = lds.davisHw.stormRain;

        updatePlot();
    }
}

void RainfallWidget::newSample(Sample sample) {
    QDate today = lastUpdate; // QDate::currentDate();
    QDate date = sample.timestamp.date();


    if (date.year() < today.year()) {
        return; // Data too old
    }

    if (date.year() > today.year()) {
        // New year. Reset all totals.
        day = 0;
        month = 0;
        year = 0;
    } else if (date.month() > today.month()) {
        // New month.
        day = 0;
        month = 0;
    } else if (date.day() > today.day()) {
        // New day
        day = 0;
    }

    lastUpdate = date;
    day += sample.rainfall;
    month += sample.rainfall;
    year += sample.rainfall;

    // Clear our rainfall guess that we computed from storm rain now that
    // we have the real data.
    rainExtra = 0;

    updatePlot();
}

void RainfallWidget::setRain(QDate date, double day, double month, double year) {
    this->lastUpdate = date;
    this->day = day;
    this->month = month;
    this->year = year;

    updatePlot();
}

double roundToMultiple(double num, double multiple) {
    return ceil(num / multiple) * multiple;
}

void RainfallWidget::updatePlot() {
    QVector<double> shortRangeValues, shortRangeTicks,
            longRangeValues, longRangeTicks;

    // Add on our guess rainfall based on changes in storm rain since the last
    // sample
    double dayValue = day + rainExtra,
            monthValue = month + rainExtra,
            yearValue = year + rainExtra,
            stormValue = storm,
            rateValue = rate;

    int decimalPlaces = 1;

    if (imperial) {
        dayValue = UnitConversions::millimetersToInches(dayValue);
        monthValue = UnitConversions::millimetersToInches(monthValue);
        yearValue = UnitConversions::millimetersToInches(yearValue);
        stormValue = UnitConversions::millimetersToInches(stormValue);
        rateValue = UnitConversions::millimetersToInches(rateValue);
        decimalPlaces = 2;
    }

    shortRangeValues << dayValue;

    if (stormRateEnabled) {
        shortRangeValues << stormValue << rateValue;
    }

    shortRangeTicks << K_DAY;
    if (stormRateEnabled) {
        shortRangeTicks << K_STORM << K_RATE;
    }

    longRangeValues << monthValue << yearValue;
    longRangeTicks << K_MONTH << K_YEAR;

    shortRangeBottomTicker->addTick(K_DAY, QString::number(dayValue, 'f', decimalPlaces));
    shortRangeBottomTicker->addTick(K_STORM, QString::number(stormValue, 'f', decimalPlaces));
    shortRangeBottomTicker->addTick(K_RATE, QString::number(rateValue, 'f', decimalPlaces));
    longRangeBottomTicker->addTick(K_MONTH, QString::number(monthValue, 'f', decimalPlaces));
    longRangeBottomTicker->addTick(K_YEAR, QString::number(yearValue, 'f', decimalPlaces));

    shortRange->setData(shortRangeTicks, shortRangeValues);
    longRange->setData(longRangeTicks, longRangeValues);

    shortRange->rescaleValueAxis();
    longRange->rescaleValueAxis();

    if (imperial) {
        shortRange->valueAxis()->setRange(
                    0, roundToMultiple(shortRange->valueAxis()->range().upper, 0.5));
        longRange->valueAxis()->setRange(
                    0, roundToMultiple(longRange->valueAxis()->range().upper, 5));
    } else {
        shortRange->valueAxis()->setRange(
                    0, roundToMultiple(shortRange->valueAxis()->range().upper, 10));
        longRange->valueAxis()->setRange(
                    0, roundToMultiple(longRange->valueAxis()->range().upper, 100));
    }
    plot->replot();
}

void RainfallWidget::plottableDoubleClick(QCPAbstractPlottable* plottable,
                                          int dataIndex,
                                          QMouseEvent* event) {
    Q_UNUSED(event);

    QCPBars *bar = qobject_cast<QCPBars*>(plottable);
    if (bar) {
        doPlot(bar == shortRange, plottable->interface1D()->dataMainKey(dataIndex));
    }
}

void RainfallWidget::doPlot(bool shortRange, int type, bool runningTotal) {
    DataSet ds;
    ds.columns.standard = SC_Rainfall;

    QTime startTime = QTime(0,0,0);
    QTime endTime = QTime(23,59,59);

    if (shortRange) {
        if (type == K_DAY || type == K_RATE) {
            ds.startTime = QDateTime::currentDateTime();
            ds.startTime.setTime(startTime);
            ds.endTime = QDateTime::currentDateTime();
            ds.endTime.setTime(endTime);

            if (type == K_RATE) {
                ds.title = QString(tr("High rain rate for %0")).arg(ds.startTime.date().toString());
            } else {
                ds.title = QString(tr("Rainfall for %0")).arg(ds.startTime.date().toString());
            }
        } else if (type == K_STORM) {
            if (!stormValid) {
                return; // No storm to plot.
            }

            ds.startTime = QDateTime(stormStart, startTime);
            ds.endTime = QDateTime::currentDateTime();
            ds.title = QString(tr("Storm starting %0")).arg(stormStart.toString());
        }

        if (type == K_RATE) {
            ds.columns.standard = SC_HighRainRate;
            ds.aggregateFunction = AF_None;
            ds.groupType = AGT_None;
            ds.customGroupMinutes = 0;
        } else {
            ds.aggregateFunction = runningTotal ? AF_RunningTotal : AF_Sum;

            // Short-ranged plots are grouped hourly
            ds.groupType = AGT_Custom;
            ds.customGroupMinutes = runningTotal ? 5 : 60;
        }
    } else {
        if (type == K_MONTH) {
            QDate today = QDateTime::currentDateTime().date();

            ds.startTime = QDateTime(QDate(today.year(), today.month(), 1), startTime);
            ds.endTime = QDateTime(today, endTime);
            ds.endTime = ds.endTime.addMonths(1);
            ds.endTime = ds.endTime.addDays(-1);
            ds.title = QString(tr("Rain for %0").arg(ds.startTime.date().toString("MMMM yyyy")));
        } else if (type == K_YEAR) {
            int year = QDateTime::currentDateTime().date().year();
            ds.startTime = QDateTime(QDate(year, 1, 1), startTime);
            ds.endTime = QDateTime(QDate(year, 12, 31), endTime);
            ds.title = QString(tr("Rain for %0")).arg(year);
        }

        ds.aggregateFunction = runningTotal ? AF_RunningTotal : AF_Sum;
        ds.groupType = AGT_Custom;
        ds.customGroupMinutes = 60;
    }

    emit chartRequested(ds);
}

void RainfallWidget::showContextMenu(QPoint point) {
    QMenu *menu = new QMenu(this);
    menu->setAcceptDrops(Qt::WA_DeleteOnClose);

    QAction *action;

    menu->addAction(tr("Copy"), this, SLOT(copy()));
    menu->addAction(tr("Save As..."), this, SLOT(save()));
    menu->addSeparator();

    menu->addAction(tr("&Refresh"), this, SLOT(requestRefresh()));
    menu->addSeparator();

    QMenu *rain = menu->addMenu(tr("Plot"));
    action = rain->addAction(tr("Today"), this, SLOT(plotRain()));
    action->setData("today,s");

    if (stormRateEnabled) {
        if (stormValid) {
            action = rain->addAction(tr("Storm"), this, SLOT(plotRain()));
            action->setData("storm,s");
        }
        action = rain->addAction(tr("High Rain Rate"), this, SLOT(plotRain()));
        action->setData("rate,s");
    }
    action = rain->addAction(tr("This Month"), this, SLOT(plotRain()));
    action->setData("month,s");
    action = rain->addAction(tr("This Year"), this, SLOT(plotRain()));
    action->setData("year,s");

    QMenu *runningTotals = menu->addMenu(tr("Plot Cumulative"));
    action = runningTotals->addAction(tr("Today"), this, SLOT(plotRain()));
    action->setData("today,c");
    if (stormValid && stormRateEnabled) {
        action = runningTotals->addAction(tr("Storm"), this, SLOT(plotRain()));
        action->setData("storm,c");
    }
    action = runningTotals->addAction(tr("This Month"), this, SLOT(plotRain()));
    action->setData("month,c");
    action = runningTotals->addAction(tr("This Year"), this, SLOT(plotRain()));
    action->setData("year,c");

    menu->popup(plot->mapToGlobal(point));
}

void RainfallWidget::requestRefresh() {
    emit refreshRequested();
}

void RainfallWidget::plotRain() {
    if (QAction* menuAction = qobject_cast<QAction*>(sender())) {
        QString data = menuAction->data().toString();

        QStringList bits = data.split(",");

        QString period = bits.first();
        bool running_total = bits.last() == "c";

        if (period == "today") {
            doPlot(true, K_DAY, running_total);
        } else if (period == "storm") {
            doPlot(true, K_STORM, running_total);
        } else if (period == "rate") {
            doPlot(true, K_RATE, running_total);
        } else if (period == "month") {
            doPlot(false, K_MONTH, running_total);
        } else if (period == "year") {
            doPlot(false, K_YEAR, running_total);
        }
    }
}

void RainfallWidget::save() {
    QString pdfFilter = "Adobe Portable Document Format (*.pdf)";
    QString pngFilter = "Portable Network Graphics (*.png)";
    QString jpgFilter = "JPEG (*.jpg)";
    QString bmpFilter = "Windows Bitmap (*.bmp)";

    QString filter = pngFilter + ";;" + pdfFilter + ";;" +
            jpgFilter + ";;" + bmpFilter;

    QString selectedFilter;

    QString fileName = QFileDialog::getSaveFileName(
                this, "Save As" ,"", filter, &selectedFilter);

    if (selectedFilter == pdfFilter)
        plot->savePdf(fileName);
    else if (selectedFilter == pngFilter)
        plot->savePng(fileName);
    else if (selectedFilter == jpgFilter)
        plot->saveJpg(fileName);
    else if (selectedFilter == bmpFilter)
        plot->saveBmp(fileName);
}

void RainfallWidget::copy() {
    QClipboard * clipboard = QApplication::clipboard();
    clipboard->setPixmap(plot->toPixmap());
}
