#include "livedatawidget.h"
#include "ui_livedatawidget.h"
#include "settings.h"
#include "datasource/databasedatasource.h"
#include "datasource/webdatasource.h"
#include "datasource/tcplivedatasource.h"

#include <QTimer>
#include <QFile>
#include <QTextStream>

#define CHECK_BIT(byte, bit) (((byte >> bit) & 0x01) == 1)

QStringList windDirections;

LiveDataWidget::LiveDataWidget(QWidget *parent) :
    QWidget(parent),
    ui(new Ui::LiveDataWidget)
{
    ui->setupUi(this);

    int row = 0;

    seconds_since_last_refresh = 0;
    minutes_late = 0;
    updateCount = 0;

    ldTimer = new QTimer(this);
    ldTimer->setInterval(1000);
    connect(ldTimer, SIGNAL(timeout()), this, SLOT(liveTimeout()));

    windDirections << "N" << "NNE" << "NE" << "ENE" << "E" << "ESE" << "SE"
                   << "SSE" << "S" << "SSW" << "SW" << "WSW" << "W" << "WNW"
                   << "NW" << "NNW";

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
    ui->lblTimestamp->setText(lds.timestamp.toString("h:mm AP"));


    /*
function BearingToCP(bearing: integer): string;
// Converts bearing in degrees to compass point
var
  compassp: array[0..15] of string = ('N', 'NNE', 'NE', 'ENE', 'E',
    'ESE', 'SE', 'SSE', 'S', 'SSW', 'SW', 'WSW', 'W', 'WNW', 'NW', 'NNW');
begin
  Result := compassp[(((bearing * 100) + 1125) mod 36000) div 2250];
end;
*/
    int idx = (((lds.windDirection * 100) + 1125) % 36000) / 2250;
    QString direction = windDirections.at(idx);

    if (lds.windSpeed == 0.0)
        ui->lblWindDirection->setText("--");
    else
        ui->lblWindDirection->setText(QString::number(lds.windDirection) +
                                      "\xB0 " + direction);

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
                                    'f', 2) + " V");
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
            ui->lblForecastIcon->setPixmap(QPixmap(iconFile));
        } else {
            ui->lblForecastIcon->setPixmap(QPixmap());
        }

        ui->lblForecast->setText(forecastRules[lds.davisHw.forecastRule]);

        updateCount++;
        ui->lblUpdateCount->setText(QString::number(updateCount));

        // I can't find anything that explains the transmitter battery status
        // byte but what I can find suggests that it gives the status for
        // transmitters 1-8. I'm assuming it must be a bitmap.
        QString txStatus = "bad: ";
        char txStatusByte = (char)lds.davisHw.txBatteryStatus;
        for (int i = 0; i < 8; i++) {
            if (CHECK_BIT(txStatusByte, i))
                txStatus.append(QString::number(i) + ", ");
        }
        if (txStatus == "bad: ")
            txStatus = "ok";
        else
            txStatus = txStatus.mid(0, txStatus.length() - 2);
        ui->lblTxBattery->setText(txStatus);

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
