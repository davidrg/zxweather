#include "forecastwidget.h"
#include "ui_forecastwidget.h"

#include <QFile>
#include <QTextStream>

ForecastWidget::ForecastWidget(QWidget *parent) :
    QWidget(parent),
    ui(new Ui::ForecastWidget)
{
    ui->setupUi(this);
    loadForecastRules();
}

ForecastWidget::~ForecastWidget()
{
    delete ui;
}

void ForecastWidget::loadForecastRules() {

    // By marking the filename as translatable we should be able
    // to swap out the built-in english version of the forecast
    // rules with an external translated json version. This is
    // untested.
    QString filename = tr(":/data/forecast_rules");

    QFile f(filename);
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

void ForecastWidget::refreshLiveData(LiveDataSet lds) {
    if (lds.hw_type != HW_DAVIS)
        return;

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
        iconFile = "partly_cloudy_snow";
        break;
    case 23:
        iconFile = "partly_cloudy_snow_or_rain";
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
}
