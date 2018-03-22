#include "rainfallwidget.h"

#include <QLabel>
#include <QFrame>
#include <QGridLayout>
#include <QTemporaryFile>
#include <QApplication>
#include <QUrl>
#include <QDesktopServices>
#include <QDir>

#include "charts/qcp/qcustomplot.h"

#define K_DAY 1
#define K_STORM 2
#define K_RATE 3
#define K_MONTH 1
#define K_YEAR 2

RainfallWidget::RainfallWidget(QWidget *parent) : QWidget(parent)
{
    // Create the basic UI
    plot = new QCustomPlot(this);
    plot->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Expanding);

    connect(plot, SIGNAL(mousePress(QMouseEvent*)),
            this, SLOT(mousePressEventSlot(QMouseEvent*)));
    connect(plot, SIGNAL(mouseMove(QMouseEvent*)),
            this, SLOT(mouseMoveEventSlot(QMouseEvent*)));
    connect(plot, SIGNAL(plottableDoubleClick(QCPAbstractPlottable*,QMouseEvent*)),
            this, SLOT(plottableDoubleClick(QCPAbstractPlottable*,QMouseEvent*)));
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
    //l->addWidget(plot, 2, 0);
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
    QVector<double> smallTicks;
    smallTicks << K_DAY << K_STORM << K_RATE;
    QVector<QString> smallTickTopLabels;
    smallTickTopLabels << "Day" << "Storm" << "Rate";
    QVector<QString> smallTickBottomLabels;
    smallTickBottomLabels << "0" << "0" << "0";

    smallRect->axis(QCPAxis::atBottom)->setAutoTicks(false);
    smallRect->axis(QCPAxis::atBottom)->setAutoTickLabels(false);
    smallRect->axis(QCPAxis::atBottom)->setTickVector(smallTicks);
    smallRect->axis(QCPAxis::atBottom)->setTickVectorLabels(smallTickBottomLabels);
    smallRect->axis(QCPAxis::atBottom)->setSubTickLength(0,0); // hide subticks

    smallRect->axis(QCPAxis::atTop)->setTickLabels(true);
    smallRect->axis(QCPAxis::atTop)->setAutoTicks(false);
    smallRect->axis(QCPAxis::atTop)->setAutoTickLabels(false);
    smallRect->axis(QCPAxis::atTop)->setTickVector(smallTicks);
    smallRect->axis(QCPAxis::atTop)->setTickVectorLabels(smallTickTopLabels);
    smallRect->axis(QCPAxis::atTop)->setTickLength(0,0); // hide ticks
    smallRect->axis(QCPAxis::atTop)->setSubTickLength(0,0); // as above

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
    QVector<double> largeTicks;
    largeTicks << K_MONTH << K_YEAR;
    QVector<QString> largeTickTopLabels;
    largeTickTopLabels << "Month" << "Year";
    QVector<QString> largeTickBottomLabels;
    largeTickBottomLabels << "0" << "0";

    largeRect->axis(QCPAxis::atBottom)->setAutoTicks(false);
    largeRect->axis(QCPAxis::atBottom)->setAutoTickLabels(false);
    largeRect->axis(QCPAxis::atBottom)->setTickVector(largeTicks);
    largeRect->axis(QCPAxis::atBottom)->setTickVectorLabels(largeTickBottomLabels);
    largeRect->axis(QCPAxis::atBottom)->setSubTickLength(0,0); // hide subticks

    largeRect->axis(QCPAxis::atTop)->setTickLabels(true);
    largeRect->axis(QCPAxis::atTop)->setAutoTicks(false);
    largeRect->axis(QCPAxis::atTop)->setAutoTickLabels(false);
    largeRect->axis(QCPAxis::atTop)->setTickVector(largeTicks);
    largeRect->axis(QCPAxis::atTop)->setTickVectorLabels(largeTickTopLabels);
    largeRect->axis(QCPAxis::atTop)->setTickLength(0,0); // hide ticks
    largeRect->axis(QCPAxis::atTop)->setSubTickLength(0,0); // as above

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

    shortRange->rescaleKeyAxis();
    longRange->rescaleKeyAxis();

    plot->replot();

    // Create a temporary filename for use in drag&drop operations
#if QT_VERSION >= 0x050000
        QString filename = QStandardPaths::writableLocation(
                    QStandardPaths::CacheLocation);
#else
        QString filename = QDesktopServices::storageLocation(
                    QDesktopServices::TempLocation);
#endif

    QTemporaryFile f(filename + QDir::separator() + "XXXXXX.png");
    f.setAutoRemove(false);
    f.open();
    tempFileName = f.fileName();
}

RainfallWidget::~RainfallWidget() {
    QFile(tempFileName).remove();
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
    lastUpdate = QDate::currentDate();

    shortRange->valueAxis()->setRange(0, 10);
    longRange->valueAxis()->setRange(0, 100);

    updatePlot();
}

void RainfallWidget::setStormRateEnabled(bool enabled) {
    if (enabled == stormRateEnabled) {
        return; // no change.
    }

    stormRateEnabled = enabled;

    QVector<QString> smallTickTopLabels;
    smallTickTopLabels << "Day";

    if (enabled) {
        smallTickTopLabels  << "Storm" << "Rate";
    }

    shortRange->valueAxis()->axisRect()->axis(QCPAxis::atTop)->setTickVectorLabels(smallTickTopLabels);

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

    updatePlot();
}

void RainfallWidget::setRain(QDate date, double day, double month, double year) {
    this->lastUpdate = date;
    this->day = day;
    this->month = month;
    this->year = year;

    updatePlot();
}

int roundToMultiple(int num, int multiple) {
    if (num == 0) {
        return multiple;
    }

    int remainder = num % multiple;

    if (remainder == 0) {
        return num;
    }

    return num + multiple - remainder;
}

void RainfallWidget::updatePlot() {
    QVector<double> shortRangeValues, shortRangeTicks,
            longRangeValues, longRangeTicks;
    QVector<QString> shortRangeTickLabels, longRangeTickLabels;

    shortRangeValues << day;

    if (stormRateEnabled) {
        shortRangeValues << storm << rate;
    }

    shortRangeTicks << K_DAY;
    if (stormRateEnabled) {
        shortRangeTicks << K_STORM << K_RATE;
    }

    shortRangeTickLabels.append(QString::number(day));

    if (stormRateEnabled) {
        shortRangeTickLabels.append(QString::number(storm));
        shortRangeTickLabels.append(QString::number(rate));
    }

    longRangeValues << month << year;
    longRangeTicks << K_MONTH << K_YEAR;
    longRangeTickLabels << QString::number(month, 'f', 1)
                        << QString::number(year, 'f', 1);

    shortRange->keyAxis()->setTickVectorLabels(shortRangeTickLabels);
    shortRange->setData(shortRangeTicks, shortRangeValues);

    longRange->keyAxis()->setTickVectorLabels(longRangeTickLabels);
    longRange->setData(longRangeTicks, longRangeValues);

    shortRange->rescaleValueAxis();
    longRange->rescaleValueAxis();

    shortRange->valueAxis()->setRange(
                0, roundToMultiple(shortRange->valueAxis()->range().upper, 10));
    longRange->valueAxis()->setRange(
                0, roundToMultiple(longRange->valueAxis()->range().upper, 100));

    plot->replot();
}

void RainfallWidget::plottableDoubleClick(QCPAbstractPlottable* plottable,
                                          QMouseEvent* event) {

    QCPBars *bar = qobject_cast<QCPBars*>(plottable);
    if (bar) {
        qDebug() << "bar!";

        if (bar == shortRange) {
            qDebug() << "short range";
            float key = shortRange->keyAxis()->pixelToCoord(event->localPos().x());
            qDebug() << key;
            if (key < K_DAY + 0.5) {
                qDebug() << "Day";
                doPlot(true, K_DAY);
            } else if (key < K_STORM + 0.5 && stormRateEnabled) {
                qDebug() << "Storm";
                doPlot(true, K_STORM);
            } else if (key < K_RATE + 0.5 && stormRateEnabled) {
                qDebug() << "Rate";
                doPlot(true, K_RATE);
            }
        } else {
            qDebug() << "long range";
            float key = longRange->keyAxis()->pixelToCoord(event->localPos().x());
            qDebug() << key;
            if (key < K_MONTH + 0.5) {
                qDebug() << "Month";
                doPlot(false, K_MONTH);
            } else if (key < K_YEAR + 0.5) {
                qDebug() << "Year";
                doPlot(false, K_YEAR);
            }

        }
    }
}

void RainfallWidget::doPlot(bool shortRange, int type, bool runningTotal) {
    DataSet ds;
    ds.columns = SC_Rainfall;

    QTime startTime = QTime(0,0,0,0);
    QTime endTime = QTime(23,59,59,59);

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
            ds.columns = SC_HighRainRate;
            ds.aggregateFunction = AF_Maximum;
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
            ds.endTime.addMonths(1);
            ds.endTime.addDays(-1);
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
