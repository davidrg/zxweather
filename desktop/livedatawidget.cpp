#include "livedatawidget.h"
#include "ui_livedatawidget.h"
#include "settings.h"
#include "datasource/databasedatasource.h"
#include "datasource/webdatasource.h"
#include "datasource/tcplivedatasource.h"

#include <QTimer>

QStringList windDirections;

LiveDataWidget::LiveDataWidget(QWidget *parent) :
    QWidget(parent),
    ui(new Ui::LiveDataWidget)
{
    ui->setupUi(this);

    windDirections << "N" << "NNE" << "NE" << "ENE" << "E" << "ESE" << "SE"
                   << "SSE" << "S" << "SSW" << "SW" << "WSW" << "W" << "WNW"
                   << "NW" << "NNW";
}

LiveDataWidget::~LiveDataWidget()
{
    delete ui;
}

void LiveDataWidget::refreshLiveData(LiveDataSet lds) {
    refreshUi(lds);
    refreshSysTrayText(lds);
    refreshSysTrayIcon(lds);
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

        ui->lblRainRate->setText(
                    QString::number(lds.davisHw.rainRate, 'f', 1) + " mm/hr");
        ui->lblCurrentStormRain->setText(
                    QString::number(lds.davisHw.stormRain, 'f', 1) + " mm");

        if (lds.davisHw.stormDateValid)
            ui->lblCurrentStormStartDate->setText(
                        lds.davisHw.stormStartDate.toString());
        else
            ui->lblCurrentStormStartDate->setText("--");

    } else {
        ui->lblRainRate->setText("not supported");
        ui->lblCurrentStormRain->setText("not supported");
        ui->lblCurrentStormStartDate->setText("not supported");
    }

    ui->lblBarometer->setText(
                QString::number(lds.pressure, 'f', 1) + " hPa" + pressureMsg);
}
