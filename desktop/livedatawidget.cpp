#include "livedatawidget.h"

#include <QLabel>
#include <QGridLayout>
#include <QFrame>
#include <QIcon>
#include <QTimer>

#include "settings.h"
#include "datasource/webdatasource.h"
#include "datasource/databasedatasource.h"

// Turns out I'm lazy.
#define GRID_ROW(left, right, name) \
    left = new QLabel(name, this); \
    right = new QLabel(this); \
    gridLayout->addWidget(left, row, 0); \
    gridLayout->addWidget(right, row, 1); \
    row++;


LiveDataWidget::LiveDataWidget(QWidget *parent) :
    QWidget(parent)
{
    gridLayout = new QGridLayout(this);

    int row = 0;
    GRID_ROW(lblTitle, lblTimestamp, "<b>Current Conditions</b>");
    lblTimestamp->setAlignment(Qt::AlignRight|Qt::AlignTrailing|Qt::AlignVCenter);
    lblTimestamp->setText("No Data");

    line = new QFrame(this);
    line->setObjectName(QString::fromUtf8("line"));
    line->setFrameShape(QFrame::HLine);
    line->setFrameShadow(QFrame::Sunken);
    gridLayout->addWidget(line, row, 0, 1, 2);
    row++;

    GRID_ROW(lblLabelRelativeHumidity, lblRelativeHumidity, "Relative Humidity:");
    GRID_ROW(lblLabelTemperature, lblTemperature, "Temperature:");
    GRID_ROW(lblLabelApparentTemperature, lblApparentTemperature, "Apparent Temperature:");
    GRID_ROW(lblLabelWindChill, lblWindChill, "Wind Chill:");
    GRID_ROW(lblLabelDewPoint, lblDewPoint, "Dew Point:");
    GRID_ROW(lblLabelAbsolutePressure, lblAbsolutePressure, "Absolute Pressure:");
    GRID_ROW(lblLabelAverageWindSpeed, lblAverageWindSpeed, "Average Wind Speed:");
    GRID_ROW(lblLabelGustWindSpeed, lblGustWindSpeed, "Gust Wind Speed:");
    GRID_ROW(lblLabelWindDirection, lblWindDirection, "Wind Direction:");


    gridLayout->setMargin(0);
    setLayout(gridLayout);

    seconds_since_last_refresh = 0;
    minutes_late = 0;

    ldTimer = new QTimer(this);
    ldTimer->setInterval(1000);
    connect(ldTimer, SIGNAL(timeout()), this, SLOT(liveTimeout()));
}

void LiveDataWidget::liveDataRefreshed(LiveDataSet lds) {
    refreshUi(lds);
    refreshSysTrayText(lds);
    refreshSysTrayIcon(lds);

    seconds_since_last_refresh = 0;
    minutes_late = 0;
}

void LiveDataWidget::refreshSysTrayText(LiveDataSet lds) {
    QString iconText, formatString;

    // Tool Tip Text
    if (lds.indoorDataAvailable) {
        formatString = "Temperature: %1\xB0" "C (%2\xB0" "C inside)\nHumidity: %3% (%4% inside)";
        iconText = formatString
                .arg(QString::number(lds.temperature, 'f', 1),
                     QString::number(lds.indoorTemperature, 'f', 1),
                     QString::number(lds.humidity, 'f', 1),
                     QString::number(lds.indoorHumidity, 'f', 1));
    } else {
        formatString = "Temperature: %1\xB0" "C\nHumidity: %3%";
        iconText = formatString
                .arg(QString::number(lds.temperature, 'f', 1),
                     QString::number(lds.humidity, 'f', 1));
    }

    if (iconText != previousSysTrayText) {
        emit sysTrayTextChanged(iconText);
        previousSysTrayText = iconText;
    }
}

void LiveDataWidget::refreshSysTrayIcon(LiveDataSet lds) {
    QString newIcon;
    if (lds.temperature > 0)
        newIcon = ":/icons/systray_icon";
    else
        newIcon = ":/icons/systray_subzero";

    if (newIcon != previousSysTrayIcon) {
        emit sysTrayIconChanged(QIcon(newIcon));
        previousSysTrayIcon = newIcon;
    }
}

void LiveDataWidget::refreshUi(LiveDataSet lds) {

    QString formatString, temp;

    // Relative Humidity
    if (lds.indoorDataAvailable) {
        formatString = "%1% (%2% inside)";
        temp = formatString
                .arg(QString::number(lds.humidity))
                .arg(QString::number(lds.indoorHumidity));
    } else {
        formatString = "%1%";
        temp = formatString
                .arg(QString::number(lds.humidity));
    }
    lblRelativeHumidity->setText(temp);

    // Temperature
    if (lds.indoorDataAvailable) {
        formatString = "%1\xB0" "C (%2\xB0" "C inside)";
        temp = formatString
                .arg(QString::number(lds.temperature,'f',1))
                .arg(QString::number(lds.indoorTemperature, 'f', 1));
    } else {
        formatString = "%1\xB0" "C";
        temp = formatString
                .arg(QString::number(lds.temperature,'f',1));
    }
    lblTemperature->setText(temp);

    lblDewPoint->setText(QString::number(lds.dewPoint, 'f', 1) + "\xB0" "C");
    lblWindChill->setText(QString::number(lds.windChill, 'f', 1) + "\xB0" "C");
    lblApparentTemperature->setText(
                QString::number(lds.apparentTemperature, 'f', 1) + "\xB0" "C");
    lblAbsolutePressure->setText(
                QString::number(lds.pressure, 'f', 1) + " hPa");
    lblAverageWindSpeed->setText(
                QString::number(lds.windSpeed, 'f', 1) + " m/s");
    lblGustWindSpeed->setText(
                QString::number(lds.gustWindSpeed, 'f', 1) + " m/s");
    lblWindDirection->setText(QString::number(lds.windDirection));
    lblTimestamp->setText(lds.timestamp.toString("h:mm AP"));
}

void LiveDataWidget::reconfigureDataSource() {
    if (Settings::getInstance().liveDataSourceType() == Settings::DS_TYPE_DATABASE) {
        dataSource.reset(new DatabaseDataSource(this,this));
    } else {
        dataSource.reset(new WebDataSource(this,this));
    }
    connect(dataSource.data(), SIGNAL(liveData(LiveDataSet)),
            this, SLOT(liveDataRefreshed(LiveDataSet)));
    connect(dataSource.data(), SIGNAL(error(QString)),
            this, SLOT(error(QString)));
    dataSource->enableLiveData();
    seconds_since_last_refresh = 0;
    ldTimer->start();
}

void LiveDataWidget::liveTimeout() {
    seconds_since_last_refresh++; // this is reset when ever live data arrives.

    if (seconds_since_last_refresh == 60) {
        minutes_late++;

        emit warning("Live data has not been refreshed in over " +
                         QString::number(minutes_late) +
                         " minutes. Check data update service.",
                         "Live data is late",
                         "Live data is late",
                         true);

        seconds_since_last_refresh = 0;
    }
}

void LiveDataWidget::error(QString message) {
    emit warning(message, "Error", "", false);
}
