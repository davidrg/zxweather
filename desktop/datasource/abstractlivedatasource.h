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

/** This is an interface for all data sources that can provide live data.
 * Data sources that can also provide samples should inherit from
 * AbstractDataSource instead.
 */
class AbstractLiveDataSource : public QObject
{
    Q_OBJECT

public:
    explicit AbstractLiveDataSource(QObject* parent=0): QObject(parent) {}

    /** Enables the live data feed. The data source will not supply any live
     * data until this is called.
     */
    virtual void enableLiveData() = 0;
signals:
    /** Emitted when ever live data is available.
     *
     * @param data The live data.
     */
    void liveData(LiveDataSet data);

    /** Emitted when ever an error occurs.
     *
     * @param errMsg The error message.
     */
    void error(QString errMsg);
};

#endif // ABSTRACTLIVEDATASOURCE_H
