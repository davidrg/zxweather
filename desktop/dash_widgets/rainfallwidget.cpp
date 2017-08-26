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

RainfallWidget::RainfallWidget(QWidget *parent) : QWidget(parent)
{
    // Create the basic UI
    plot = new QCustomPlot(this);
    plot->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Expanding);

    connect(plot, SIGNAL(mousePress(QMouseEvent*)),
            this, SLOT(mousePressEventSlot(QMouseEvent*)));
    connect(plot, SIGNAL(mouseMove(QMouseEvent*)),
            this, SLOT(mouseMoveEventSlot(QMouseEvent*)));

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
    smallTicks << 1 << 2 << 3;
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
    largeTicks << 1 << 2;
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

    shortRangeTicks << 1;
    if (stormRateEnabled) {
        shortRangeTicks << 2 << 3;
    }

    shortRangeTickLabels.append(QString::number(day));

    if (stormRateEnabled) {
        shortRangeTickLabels.append(QString::number(storm));
        shortRangeTickLabels.append(QString::number(rate));
    }

    longRangeValues << month << year;
    longRangeTicks << 1 << 2;
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
