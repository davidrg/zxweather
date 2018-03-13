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
//    ui->lblUVIndex->hide();
//    ui->lblSolarRadiation->hide();
//    ui->solarRadiation->hide();
//    ui->uvIndex->hide();
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

    QString bft = "";
    if (lds.windSpeed < 0.3) // 0
        bft = "calm";
    else if (lds.windSpeed < 2) // 1
        bft = "light air";
    else if (lds.windSpeed < 3) // 2
        bft = "light breeze";
    else if (lds.windSpeed < 5.4) // 3
        bft = "gentle breeze";
    else if (lds.windSpeed < 8) // 4
        bft = "moderate breeze";
    else if (lds.windSpeed < 10.7) // 5
        bft = "fresh breeze";
    else if (lds.windSpeed < 13.8) // 6
        bft = "strong breeze";
    else if (lds.windSpeed < 17.1) // 7
        bft = "high wind, near gale";
    else if (lds.windSpeed < 20.6) // 8
        bft = "gale, fresh gale";
    else if (lds.windSpeed < 24.4) // 9
        bft = "strong gale";
    else if (lds.windSpeed < 28.3) // 10
        bft = "storm, whole gale";
    else if (lds.windSpeed < 32.5) // 11
        bft = "violent storm";
    else // 12
        bft = "hurricane";

    if (!bft.isEmpty())
        bft = " (" + bft + ")";

    ui->lblWindSpeed->setText(
                QString::number(lds.windSpeed, 'f', 1) + " m/s" + bft);
    ui->lblTimestamp->setText(lds.timestamp.toString("h:mm AP"));

    if (lds.windSpeed == 0.0)
        ui->lblWindDirection->setText("--");
    else {
        int idx = (((lds.windDirection * 100) + 1125) % 36000) / 2250;
        QString direction = windDirections.at(idx);

        ui->lblWindDirection->setText(QString::number(lds.windDirection) +
                                      "\xB0 " + direction);
    }

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

        // TODO: check if HW has solar+UV
        ui->lblUVIndex->setText(QString::number(lds.davisHw.uvIndex, 'f', 1));
        ui->lblSolarRadiation->setText(QString::number(lds.davisHw.solarRadiation)
                                       + " W/m\xB2");
        ui->lblRainRate->show();
        ui->lblCurrentStormRain->show();
        ui->lblCurrentStormStartDate->show();
        ui->rainRate->show();
        ui->currentStormRain->show();
        ui->currentStormStart->show();

    } else {
        ui->lblRainRate->setText("not supported");
        ui->lblCurrentStormRain->setText("not supported");
        ui->lblCurrentStormStartDate->setText("not supported");
        ui->lblUVIndex->setText("not supported");
        ui->lblSolarRadiation->setText("not supported");

        ui->lblRainRate->hide();
        ui->lblCurrentStormRain->hide();
        ui->lblCurrentStormStartDate->hide();
        ui->lblUVIndex->hide();
        ui->lblSolarRadiation->hide();
        ui->rainRate->hide();
        ui->currentStormRain->hide();
        ui->currentStormStart->hide();
    }

    ui->lblBarometer->setText(
                QString::number(lds.pressure, 'f', 1) + " hPa" + pressureMsg);
}

void LiveDataWidget::setSolarDataAvailable(bool available) {
    ui->lblUVIndex->setVisible(available);
    ui->lblSolarRadiation->setVisible(available);
    ui->uvIndex->setVisible(available);
    ui->solarRadiation->setVisible(available);

    // These two numbers will keep the height of the widget correct
    // when the UV and Solar fields are shown and hidden.
//    if (available) {
//        setMinimumHeight(265);
//        setFixedHeight(265);
//    } else {
//        setMinimumHeight(232);
//        setFixedHeight(232);
//    }
    updateGeometry();
    adjustSize();
}