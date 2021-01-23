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

    imperial = Settings::getInstance().imperial();

    connect(ui->lblApparentTemperature, SIGNAL(plotRequested(DataSet)),
            this, SLOT(childPlotRequested(DataSet)));
    connect(ui->lblBarometer, SIGNAL(plotRequested(DataSet)),
            this, SLOT(childPlotRequested(DataSet)));
    connect(ui->lblDewPoint, SIGNAL(plotRequested(DataSet)),
            this, SLOT(childPlotRequested(DataSet)));
    connect(ui->lblHumidity, SIGNAL(plotRequested(DataSet)),
            this, SLOT(childPlotRequested(DataSet)));
    connect(ui->lblRainRate, SIGNAL(plotRequested(DataSet)),
            this, SLOT(childPlotRequested(DataSet)));
    connect(ui->lblSolarRadiation, SIGNAL(plotRequested(DataSet)),
            this, SLOT(childPlotRequested(DataSet)));
    connect(ui->lblStormRain, SIGNAL(plotRequested(DataSet)),
            this, SLOT(childPlotRequested(DataSet)));
    connect(ui->lblTemperature, SIGNAL(plotRequested(DataSet)),
            this, SLOT(childPlotRequested(DataSet)));
    connect(ui->lblUVIndex, SIGNAL(plotRequested(DataSet)),
            this, SLOT(childPlotRequested(DataSet)));
    connect(ui->lblWindChill, SIGNAL(plotRequested(DataSet)),
            this, SLOT(childPlotRequested(DataSet)));
    connect(ui->lblWindDirection, SIGNAL(plotRequested(DataSet)),
            this, SLOT(childPlotRequested(DataSet)));
    connect(ui->lblWindSpeed, SIGNAL(plotRequested(DataSet)),
            this, SLOT(childPlotRequested(DataSet)));

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

    if (imperial) {
        // Tool Tip Text
        if (lds.indoorDataAvailable) {
            formatString = tr("Temperature: %1" IMPERIAL_TEMPERATURE_SYMBOL " (%2" IMPERIAL_TEMPERATURE_SYMBOL " inside)\nHumidity: %3% (%4% inside)");
            iconText = formatString
                    .arg(QString::number(UnitConversions::celsiusToFahrenheit(lds.temperature), 'f', 1),
                         QString::number(UnitConversions::celsiusToFahrenheit(lds.indoorTemperature), 'f', 1),
                         QString::number(lds.humidity, 'f', 1),
                         QString::number(lds.indoorHumidity, 'f', 1));
        } else {
            formatString = tr("Temperature: %1" IMPERIAL_TEMPERATURE_SYMBOL "\nHumidity: %3%");
            iconText = formatString
                    .arg(QString::number(UnitConversions::celsiusToFahrenheit(lds.temperature), 'f', 1),
                         QString::number(lds.humidity, 'f', 1));
        }
    } else {
        // Tool Tip Text
        if (lds.indoorDataAvailable) {
            formatString = tr("Temperature: %1" TEMPERATURE_SYMBOL " (%2" TEMPERATURE_SYMBOL " inside)\nHumidity: %3% (%4% inside)");
            iconText = formatString
                    .arg(QString::number(lds.temperature, 'f', 1),
                         QString::number(lds.indoorTemperature, 'f', 1),
                         QString::number(lds.humidity, 'f', 1),
                         QString::number(lds.indoorHumidity, 'f', 1));
        } else {
            formatString = tr("Temperature: %1" TEMPERATURE_SYMBOL "\nHumidity: %3%");
            iconText = formatString
                    .arg(QString::number(lds.temperature, 'f', 1),
                         QString::number(lds.humidity, 'f', 1));
        }
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

    if (ldu.indoorDataAvailable) {
        ui->lblTemperature->setOutdoorIndoorValue(ldu.temperature, SC_Temperature,
                                                  ldu.indoorTemperature, SC_IndoorTemperature);
        ui->lblHumidity->setOutdoorIndoorValue(ldu.humidity, SC_Humidity,
                                               ldu.indoorHumidity, SC_IndoorHumidity);
    } else {
        ui->lblTemperature->setValue(ldu.temperature, SC_Temperature);
        ui->lblHumidity->setValue(ldu.humidity, SC_Humidity);
    }

    ui->lblApparentTemperature->setValue(ldu.apparentTemperature, SC_ApparentTemperature);
    ui->lblWindChill->setValue(ldu.windChill, SC_WindChill);
    ui->lblDewPoint->setValue(ldu.dewPoint, SC_DewPoint);

    if (ldu.hw_type == HW_DAVIS) {
        ui->lblBarometer->setDoubleValue(ldu.pressure, SC_Pressure,
                                         ldu.barometerTrend, SC_Pressure);
    } else {
        ui->lblBarometer->setValue(ldu.pressure, SC_Pressure);
    }

    ui->lblWindSpeed->setDoubleValue(ldu.windSpeed, SC_AverageWindSpeed,
                                     ldu.windSpeedBft, SC_AverageWindSpeed);
    ui->lblTimestamp->setText(ldu.timestamp.toString("h:mm AP"));

    if ((float)ldu.windSpeed == 0.0)
        ui->lblWindDirection->clear();
    else {
        ui->lblWindDirection->setDoubleValue(
                    ldu.windDirection, SC_WindDirection,
                    ldu.windDirectionPoint, SC_WindDirection);
    }


    if (lds.hw_type == HW_DAVIS) {
        ui->lblRainRate->setValue(ldu.rainRate, SC_HighRainRate);
        ui->lblStormRain->setValue(ldu.stormRain, SC_NoColumns);

        if (lds.davisHw.stormDateValid)
            ui->lblCurrentStormStartDate->setText(
                        lds.davisHw.stormStartDate.toString());
        else
            ui->lblCurrentStormStartDate->setText("--");

        // TODO: check if HW has solar+UV
        ui->lblUVIndex->setValue(ldu.uvIndex, SC_UV_Index);
        ui->lblSolarRadiation->setValue(ldu.solarRadiation, SC_SolarRadiation);

        ui->lblRainRate->show();
        ui->lblStormRain->show();
        ui->lblCurrentStormStartDate->show();
        ui->rainRate->show();
        ui->currentStormRain->show();
        ui->currentStormStart->show();

    } else {
        ui->lblRainRate->hide();
        ui->lblStormRain->hide();
        ui->lblCurrentStormStartDate->hide();
        ui->lblUVIndex->hide();
        ui->lblSolarRadiation->hide();
        ui->rainRate->hide();
        ui->currentStormRain->hide();
        ui->currentStormStart->hide();
    }
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

void LiveDataWidget::childPlotRequested(DataSet ds) {
    emit plotRequested(ds);
}
