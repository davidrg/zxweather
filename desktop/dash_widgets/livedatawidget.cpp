#include "livedatawidget.h"
#include "ui_livedatawidget.h"
#include "settings.h"
#include "datasource/databasedatasource.h"
#include "datasource/webdatasource.h"
#include "datasource/tcplivedatasource.h"
#include "constants.h"

#include "unit_conversions.h"


#include <QTimer>


/*
 * TODO: replace unit_t with a real object:
 *  -> Implicit type conversions for int, float and QString
 *  -> Implicit constructors from floats and ints (should remember which value
 *     was assigned and so as to preserve accuracy for ints.
 */

typedef struct _liveDataWithUnits {
    UnitConversions::UnitValue temperature;
    UnitConversions::UnitValue indoorTemperature;
    UnitConversions::UnitValue apparentTemperature;
    UnitConversions::UnitValue windChill;
    UnitConversions::UnitValue dewPoint;
    UnitConversions::UnitValue humidity;
    UnitConversions::UnitValue indoorHumidity;
    UnitConversions::UnitValue pressure;
    UnitConversions::UnitValue windSpeed;
    UnitConversions::UnitValue windSpeedBft;
    UnitConversions::UnitValue windDirection;
    UnitConversions::UnitValue windDirectionPoint;

    QDateTime timestamp;

    bool indoorDataAvailable;

    hardware_type_t hw_type;

    // Davis stuff
    UnitConversions::UnitValue stormRain;
    UnitConversions::UnitValue rainRate;
    QDate stormStartDate;
    bool stormDateValid;

    // int
    UnitConversions::UnitValue barometerTrend;

    int forecastIcon;
    int forecastRule;
    int txBatteryStatus;

    UnitConversions::UnitValue consoleBatteryVoltage;
    UnitConversions::UnitValue uvIndex;
    UnitConversions::UnitValue solarRadiation;
} LiveDataU;

LiveDataU unitifyLiveData(LiveDataSet lds) {
    LiveDataU ldu;
    ldu.temperature = lds.temperature;
    ldu.temperature.unit = UnitConversions::U_CELSIUS;

    ldu.indoorTemperature = lds.indoorTemperature;
    ldu.indoorTemperature.unit = UnitConversions::U_CELSIUS;

    ldu.apparentTemperature = lds.apparentTemperature;
    ldu.apparentTemperature.unit = UnitConversions::U_CELSIUS;

    ldu.windChill = lds.windChill;
    ldu.windChill.unit = UnitConversions::U_CELSIUS;

    ldu.dewPoint = lds.dewPoint;
    ldu.dewPoint.unit = UnitConversions::U_CELSIUS;

    ldu.humidity = lds.humidity;
    ldu.humidity.unit = UnitConversions::U_HUMIDITY;

    ldu.indoorHumidity = lds.indoorHumidity;
    ldu.indoorHumidity.unit = UnitConversions::U_HUMIDITY;

    ldu.pressure = lds.pressure;
    ldu.pressure.unit = UnitConversions::U_HECTOPASCALS;

    ldu.windSpeed = lds.windSpeed;
    ldu.windSpeed.unit = UnitConversions::U_METERS_PER_SECOND;

    ldu.windSpeedBft = UnitConversions::metersPerSecondtoBFT(lds.windSpeed);
    ldu.windSpeedBft.unit = UnitConversions::U_BFT;

    ldu.windDirection = lds.windDirection;
    ldu.windDirection.unit = UnitConversions::U_DEGREES;

    ldu.windDirectionPoint = lds.windDirection;
    ldu.windDirectionPoint.unit = UnitConversions::U_COMPASS_POINT;

    ldu.stormRain = lds.davisHw.stormRain;
    ldu.stormRain.unit = UnitConversions::U_MILLIMETERS;

    ldu.rainRate = lds.davisHw.rainRate;
    ldu.rainRate.unit = UnitConversions::U_MILLIMETERS_PER_HOUR;

    ldu.stormDateValid = lds.davisHw.stormDateValid;
    ldu.stormStartDate = lds.davisHw.stormStartDate;

    ldu.barometerTrend = lds.davisHw.barometerTrend;
    ldu.barometerTrend.unit = UnitConversions::U_DAVIS_BAROMETER_TREND;

    ldu.forecastIcon = lds.davisHw.forecastIcon;
    ldu.forecastRule = lds.davisHw.forecastRule;
    ldu.txBatteryStatus = lds.davisHw.txBatteryStatus;

    ldu.consoleBatteryVoltage = lds.davisHw.consoleBatteryVoltage;
    ldu.consoleBatteryVoltage.unit = UnitConversions::U_VOLTAGE;

    ldu.uvIndex = lds.davisHw.uvIndex;
    ldu.uvIndex.unit = UnitConversions::U_UV_INDEX;

    ldu.solarRadiation = lds.davisHw.solarRadiation;
    ldu.solarRadiation.unit = UnitConversions::U_WATTS_PER_SQUARE_METER;

    ldu.timestamp = lds.timestamp;
    ldu.indoorDataAvailable = lds.indoorDataAvailable;
    ldu.hw_type = lds.hw_type;

    return ldu;
}


LiveDataWidget::LiveDataWidget(QWidget *parent) :
    QWidget(parent),
    ui(new Ui::LiveDataWidget)
{
    ui->setupUi(this);

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
        formatString = "Temperature: %1" TEMPERATURE_SYMBOL " (%2" TEMPERATURE_SYMBOL " inside)\nHumidity: %3% (%4% inside)";
        iconText = formatString
                .arg(QString::number(lds.temperature, 'f', 1),
                     QString::number(lds.indoorTemperature, 'f', 1),
                     QString::number(lds.humidity, 'f', 1),
                     QString::number(lds.indoorHumidity, 'f', 1));
    } else {
        formatString = "Temperature: %1" TEMPERATURE_SYMBOL "\nHumidity: %3%";
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
    LiveDataU ldu = unitifyLiveData(lds);

    // Relative Humidity
    if (ldu.indoorDataAvailable) {
        ui->lblHumidity->setText(QString("%0 (%1 inside)")
                                 .arg(QString(ldu.indoorHumidity))
                                 .arg(QString(ldu.humidity)));
    } else {
        ui->lblHumidity->setText(ldu.humidity);
    }

    // Temperature
    if (ldu.indoorDataAvailable) {
        ui->lblTemperature->setText(QString("%0 (%1 inside)")
                                .arg(QString(ldu.temperature))
                                .arg(QString(ldu.indoorTemperature)));
    } else {
        ui->lblTemperature->setText(ldu.temperature);
    }

    ui->lblDewPoint->setText(ldu.dewPoint);
    ui->lblWindChill->setText(ldu.windChill);
    ui->lblApparentTemperature->setText(ldu.apparentTemperature);


    QString bft = ldu.windSpeedBft;
    if (!bft.isEmpty())
        bft = " (" + bft + ")";

    ui->lblWindSpeed->setText(QString(ldu.windSpeed) + bft);
    ui->lblTimestamp->setText(lds.timestamp.toString("h:mm AP"));

    if ((float)ldu.windSpeed == 0.0)
        ui->lblWindDirection->setText("--");
    else {
        QString direction = ldu.windDirectionPoint;

        ui->lblWindDirection->setText(QString(ldu.windDirection) + " " + direction);
    }

    QString pressureMsg = "";
    if (lds.hw_type == HW_DAVIS) {
        pressureMsg = QString(ldu.barometerTrend);

        if (!pressureMsg.isEmpty())
            pressureMsg = " (" + pressureMsg + ")";

        ui->lblRainRate->setText(ldu.rainRate);
        ui->lblCurrentStormRain->setText(ldu.stormRain);

        if (lds.davisHw.stormDateValid)
            ui->lblCurrentStormStartDate->setText(
                        lds.davisHw.stormStartDate.toString());
        else
            ui->lblCurrentStormStartDate->setText("--");

        // TODO: check if HW has solar+UV
        ui->lblUVIndex->setText(ldu.uvIndex);
        ui->lblSolarRadiation->setText(ldu.solarRadiation);
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
