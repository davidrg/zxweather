#include "livedatawidget.h"
#include "ui_livedatawidget.h"
#include "settings.h"
#include "datasource/databasedatasource.h"
#include "datasource/webdatasource.h"
#include "datasource/tcplivedatasource.h"

#include <QTimer>
#include <QFile>
#include <QTextStream>

LiveDataWidget::LiveDataWidget(QWidget *parent) :
    QWidget(parent),
    ui(new Ui::LiveDataWidget)
{
    ui->setupUi(this);

    int row = 0;

    seconds_since_last_refresh = 0;
    minutes_late = 0;

    ldTimer = new QTimer(this);
    ldTimer->setInterval(1000);
    connect(ldTimer, SIGNAL(timeout()), this, SLOT(liveTimeout()));

    loadForecastRules();

}

LiveDataWidget::~LiveDataWidget()
{
    delete ui;
}


void LiveDataWidget::loadForecastRules() {
    QFile f(":/data/forecast_rules");
    if (!f.open(QIODevice::ReadOnly))
        return;

    QTextStream in(&f);
    QString line = in.readLine();
    while (!line.isNull()) {

        QStringList bits = line.split('|');

        int id = bits.at(0).toInt();
        QString forecast = bits.at(1);

        forecastRules[id] = forecast;

        line = in.readLine();
    }
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
    ui->lblHumidity->setText(temp);

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
    ui->lblTemperature->setText(temp);

    ui->lblDewPoint->setText(QString::number(lds.dewPoint, 'f', 1) + "\xB0" "C");
    ui->lblWindChill->setText(QString::number(lds.windChill, 'f', 1) + "\xB0" "C");
    ui->lblApparentTemperature->setText(
                QString::number(lds.apparentTemperature, 'f', 1) + "\xB0" "C");


    ui->lblWindSpeed->setText(
                QString::number(lds.windSpeed, 'f', 1) + " m/s");
    ui->lblWindDirection->setText(QString::number(lds.windDirection));
    ui->lblTimestamp->setText(lds.timestamp.toString("h:mm AP"));

    QString pressureMsg = "";
    if (lds.hw_type == HW_DAVIS) {
        switch (lds.davisHw.barometerTrend) {
        case -60:
            pressureMsg = "falling rapidly";
            break;
        case -20:
            pressureMsg = "falling slowly";
            break;
        case 0:
            pressureMsg = "steady";
            break;
        case 20:
            pressureMsg = "rising slowly";
            break;
        case 60:
            pressureMsg = "rising rapidly";
            break;
        default:
            pressureMsg = "";
        }
        if (!pressureMsg.isEmpty())
            pressureMsg = " (" + pressureMsg + ")";

        ui->lblConsoleBattery->setText(
                    QString::number(lds.davisHw.consoleBatteryVoltage,
                                    'f', 1) + " V");
        ui->lblRainRate->setText(
                    QString::number(lds.davisHw.rainRate, 'f', 1) + " mm/hr");
        ui->lblCurrentStormRain->setText(
                    QString::number(lds.davisHw.stormRain, 'f', 1) + " mm");

        if (lds.davisHw.stormDateValid)
            ui->lblCurrentStormStartDate->setText(
                        lds.davisHw.stormStartDate.toString());
        else
            ui->lblCurrentStormStartDate->setText("--");

        //:/icons/weather/
        QString iconFile = "";

        switch(lds.davisHw.forecastIcon) {
        case 8:
            iconFile = "clear";
            break;
        case 6:
            iconFile = "partly_cloudy";
            break;
        case 2:
            iconFile = "mostly_cloudy";
            break;
        case 3:
            iconFile = "mostly_cloudy_rain";
            break;
        case 18:
            iconFile = "mostly_cloudy_snow";
            break;
        case 19:
            iconFile = "mostly_cloudy_snow_or_rain";
            break;
        case 7:
            iconFile = "partly_cloudy_rain";
            break;
        case 22:
            iconFile = "partly_cloudy_show";
            break;
        case 23:
            iconFile = "partly_cloudy_show_or_rain";
            break;
        default:
            iconFile = "";
            break;
        }

        if (!iconFile.isEmpty()) {
            iconFile = ":/icons/weather/" + iconFile;
        }
        // 183 is longest. Real is forecastRules[lds.davisHw.forecastRule

        ui->lblForecastIcon->setPixmap(QPixmap(iconFile));
        ui->lblForecast->setText(forecastRules[183]);
    }

    ui->lblBarometer->setText(
                QString::number(lds.pressure, 'f', 1) + " hPa" + pressureMsg);

}

void LiveDataWidget::reconfigureDataSource() {
    Settings& settings = Settings::getInstance();

    if (settings.liveDataSourceType() == Settings::DS_TYPE_DATABASE) {
        dataSource.reset(new DatabaseDataSource(this,this));
    } else if (settings.liveDataSourceType() == Settings::DS_TYPE_WEB_INTERFACE){
        dataSource.reset(new WebDataSource(this,this));
    } else {
        dataSource.reset(new TcpLiveDataSource(this));
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
