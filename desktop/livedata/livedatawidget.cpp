#include "livedatawidget.h"

#include <QLabel>
#include <QGridLayout>
#include <QFrame>
#include <QIcon>
#include <QTimer>

#include "settings.h"
#include "databaselivedatasource.h"
#include "jsonlivedatasource.h"

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

void LiveDataWidget::liveDataRefreshed() {
    QScopedPointer<AbstractLiveData> data(dataSource->getLiveData());

    refreshUi(data.data());
    refreshSysTrayText(data.data());
    refreshSysTrayIcon(data.data());

    seconds_since_last_refresh = 0;
    minutes_late = 0;
}

void LiveDataWidget::refreshSysTrayText(AbstractLiveData *data) {
    QString iconText, formatString;

    // Tool Tip Text
    if (data->indoorDataAvailable()) {
        formatString = "Temperature: %1\xB0" "C (%2\xB0" "C inside)\nHumidity: %3% (%4% inside)";
        iconText = formatString
                .arg(QString::number(data->getTemperature(), 'f', 1),
                     QString::number(data->getIndoorTemperature(), 'f', 1),
                     QString::number(data->getRelativeHumidity(), 'f', 1),
                     QString::number(data->getIndoorRelativeHumidity(), 'f', 1));
    } else {
        formatString = "Temperature: %1\xB0" "C\nHumidity: %3%";
        iconText = formatString
                .arg(QString::number(data->getTemperature(), 'f', 1),
                     QString::number(data->getRelativeHumidity(), 'f', 1));
    }

    if (iconText != previousSysTrayText) {
        emit sysTrayTextChanged(iconText);
        previousSysTrayText = iconText;
    }
}

void LiveDataWidget::refreshSysTrayIcon(AbstractLiveData *data) {
    QString newIcon;
    if (data->getTemperature() > 0)
        newIcon = ":/icons/systray_icon";
    else
        newIcon = ":/icons/systray_subzero";

    if (newIcon != previousSysTrayIcon) {
        emit sysTrayIconChanged(QIcon(newIcon));
        previousSysTrayIcon = newIcon;
    }
}

void LiveDataWidget::refreshUi(AbstractLiveData* data) {

    QString formatString, temp;

    // Relative Humidity
    if (data->indoorDataAvailable()) {
        formatString = "%1% (%2% inside)";
        temp = formatString
                .arg(QString::number(data->getRelativeHumidity()))
                .arg(QString::number(data->getIndoorRelativeHumidity()));
    } else {
        formatString = "%1%";
        temp = formatString
                .arg(QString::number(data->getRelativeHumidity()));
    }
    lblRelativeHumidity->setText(temp);

    // Temperature
    if (data->indoorDataAvailable()) {
        formatString = "%1\xB0" "C (%2\xB0" "C inside)";
        temp = formatString
                .arg(QString::number(data->getTemperature(),'f',1))
                .arg(QString::number(data->getIndoorTemperature(), 'f', 1));
    } else {
        formatString = "%1\xB0" "C";
        temp = formatString
                .arg(QString::number(data->getTemperature(),'f',1));
    }
    lblTemperature->setText(temp);

    lblDewPoint->setText(QString::number(data->getDewPoint(), 'f', 1) + "\xB0" "C");
    lblWindChill->setText(QString::number(data->getWindChill(), 'f', 1) + "\xB0" "C");
    lblApparentTemperature->setText(
                QString::number(data->getApparentTemperature(), 'f', 1) + "\xB0" "C");
    lblAbsolutePressure->setText(
                QString::number(data->getAbsolutePressure(), 'f', 1) + " hPa");
    lblAverageWindSpeed->setText(
                QString::number(data->getAverageWindSpeed(), 'f', 1) + " m/s");
    lblGustWindSpeed->setText(
                QString::number(data->getGustWindSpeed(), 'f', 1) + " m/s");
    lblWindDirection->setText(data->getWindDirection());
    lblTimestamp->setText(data->getTimestamp().toString("h:mm AP"));
}

void LiveDataWidget::reconfigureDataSource() {
    if (Settings::getInstance().dataSourceType() == Settings::DS_TYPE_DATABASE)
        createDatabaseDataSource();
    else
        createJsonDataSource();
}

void LiveDataWidget::createJsonDataSource() {
    QString url = Settings::getInstance().url();

    JsonLiveDataSource *jds = new JsonLiveDataSource(url, this);
    connect(jds, SIGNAL(networkError(QString)),
            this, SLOT(networkError(QString)));
    connect(jds, SIGNAL(liveDataRefreshed()),
            this, SLOT(liveDataRefreshed()));

    dataSource.reset(jds);
    ldTimer->start();
}

void LiveDataWidget::createDatabaseDataSource() {
    Settings& settings = Settings::getInstance();
    QString dbName = settings.databaseName();
    QString hostname = settings.databaseHostName();
    int port = settings.databasePort();
    QString username = settings.databaseUsername();
    QString password = settings.databasePassword();
    QString station = settings.stationName();

    // Kill the old datasource. The DatabaseDataSource uses named connections
    // so we can't have two overlaping.
    if (!dataSource.isNull())
        delete dataSource.take();

    QScopedPointer<DatabaseLiveDataSource> dds;

    dds.reset(new DatabaseLiveDataSource(dbName,
                                 hostname,
                                 port,
                                 username,
                                 password,
                                 station,
                                 this));

    if (!dds->isConnected()) {
        return;
    }

    connect(dds.data(), SIGNAL(connection_failed(QString)),
            this, SLOT(connection_failed(QString)));
    connect(dds.data(), SIGNAL(database_error(QString)),
            this, SLOT(unknown_db_error(QString)));
    connect(dds.data(), SIGNAL(liveDataRefreshed()),
            this, SLOT(liveDataRefreshed()));

    dataSource.reset(dds.take());
    seconds_since_last_refresh = 0;
    ldTimer->start();

    setWindowTitle("zxweather - " + station);

    // Do an initial refresh so we're not waiting forever with nothing to show
    liveDataRefreshed();
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

void LiveDataWidget::connection_failed(QString) {
    emit warning("Failed to connect to the database",
                     "Error",
                     "Database connect failed",
                     true);
    ldTimer->stop();
}

void LiveDataWidget::networkError(QString message) {
    emit warning(message, "Error", "Network Error", true);
}

void LiveDataWidget::unknown_db_error(QString message) {
    emit warning(message, "Database Error", "", false);
}
