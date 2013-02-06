#ifndef ABSTRACTLIVEDATASOURCE_H
#define ABSTRACTLIVEDATASOURCE_H

#include <QObject>
#include <QDateTime>

typedef struct _liveData {
    float temperature;
    float indoorTemperature;
    float apparentTemperature;
    float windChill;
    float dewPoint;

    int humidity;
    int indoorHumidity;

    float pressure;

    float windSpeed;
    float gustWindSpeed;
    int windDirection;

    QDateTime timestamp;

    bool indoorDataAvailable;
} LiveDataSet;

class AbstractLiveDataSource2 : public QObject
{
    Q_OBJECT

public:
    explicit AbstractLiveDataSource2(QObject* parent=0): QObject(parent) {}

    virtual void enableLiveData() = 0;
signals:
    void liveData(LiveDataSet data);
    void error(QString errMsg);
};

#endif // ABSTRACTLIVEDATASOURCE_H
