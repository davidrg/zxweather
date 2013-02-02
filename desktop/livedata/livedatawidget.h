#ifndef LIVEDATAWIDGET_H
#define LIVEDATAWIDGET_H

#include <QWidget>

class QLabel;
class QGridLayout;
class QFrame;
class AbstractLiveData;

class LiveDataWidget : public QWidget
{
    Q_OBJECT
public:
    explicit LiveDataWidget(QWidget *parent = 0);
    
    void refresh(AbstractLiveData* data);

signals:
    
public slots:
    

private:
    QLabel* lblRelativeHumidity;
    QLabel* lblTemperature;
    QLabel* lblDewPoint;
    QLabel* lblWindChill;
    QLabel* lblApparentTemperature;
    QLabel* lblAbsolutePressure;
    QLabel* lblAverageWindSpeed;
    QLabel* lblGustWindSpeed;
    QLabel* lblWindDirection;
    QLabel* lblTimestamp;

    QLabel* lblTitle;
    QLabel* lblLabelRelativeHumidity;
    QLabel* lblLabelTemperature;
    QLabel* lblLabelDewPoint;
    QLabel* lblLabelWindChill;
    QLabel* lblLabelApparentTemperature;
    QLabel* lblLabelAbsolutePressure;
    QLabel* lblLabelAverageWindSpeed;
    QLabel* lblLabelGustWindSpeed;
    QLabel* lblLabelWindDirection;
    QLabel* lblLabelTimestamp;

    QGridLayout* gridLayout;
    QFrame* line;
};

#endif // LIVEDATAWIDGET_H
