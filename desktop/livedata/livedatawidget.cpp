#include "livedatawidget.h"

#include <QLabel>
#include <QGridLayout>
#include <QFrame>
#include "livedatasource.h"

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
}

void LiveDataWidget::refresh(AbstractLiveData* data) {

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
