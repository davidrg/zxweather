/*****************************************************************************
 *            Created: 25/08/2012
 *          Copyright: (C) Copyright David Goodwin, 2012
 *            License: GNU General Public License
 *****************************************************************************
 *
 *   This is free software; you can redistribute it and/or modify
 *   it under the terms of the GNU General Public License as published by
 *   the Free Software Foundation; either version 2 of the License, or
 *   (at your option) any later version.
 *
 *   This software is distributed in the hope that it will be useful,
 *   but WITHOUT ANY WARRANTY; without even the implied warranty of
 *   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *   GNU General Public License for more details.
 *
 *   You should have received a copy of the GNU General Public License
 *   along with this software; if not, write to the Free Software
 *   Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA  02110-1301  USA
 *
 ****************************************************************************/

#ifndef DATASOURCE_H
#define DATASOURCE_H

#include <QObject>
#include <QDateTime>

/** An abstract class for providing access to live data. The LiveData class
 * is a subclass that stores the data internally.
 */
class AbstractLiveData : public QObject {
    Q_OBJECT
public:
    AbstractLiveData(QObject *parent=0) : QObject(parent) {}

    virtual float getIndoorTemperature() const = 0;
    virtual int getIndoorRelativeHumidity() const = 0;
    virtual float getTemperature() const = 0;
    virtual int getRelativeHumidity() const = 0;
    virtual float getDewPoint() const = 0;
    virtual float getWindChill() const = 0;
    virtual float getApparentTemperature() const = 0;
    virtual float getAbsolutePressure() const = 0;
    virtual float getAverageWindSpeed() const = 0;
    virtual float getGustWindSpeed() const = 0;
    virtual QString getWindDirection() const = 0;
    virtual QDateTime getTimestamp() const = 0;
    virtual bool indoorDataAvailable() const = 0;
};

/** An implementation of AbstractLiveData that stores the data internally.
 */
class LiveData : public AbstractLiveData {
    Q_OBJECT
public:

    /* Read methods */
    float getIndoorTemperature() const { return indoorTemperature; }
    int   getIndoorRelativeHumidity() const { return indoorRelativeHumidity; }
    float getTemperature() const { return temperature; }
    int   getRelativeHumidity() const { return relativeHumidity; }
    float getDewPoint() const { return dewPoint; }
    float getWindChill() const { return windChill; }
    float getApparentTemperature() const { return apparentTemperature; }
    float getAbsolutePressure() const { return absolutePressure; }
    float getAverageWindSpeed() const { return averageWindSpeed; }
    float getGustWindSpeed() const { return gustWindSpeed; }
    QString getWindDirection() const { return windDirection; }
    QDateTime getTimestamp() const { return timestamp; }
    bool indoorDataAvailable() const {return _indoorDataAvailable; }

    /* Write methods */
    void setIndoorTemperature(float value) { indoorTemperature = value; }
    void setIndoorRelativeHumidity(int value) { indoorRelativeHumidity = value; }
    void setTemperature(float value) { temperature = value; }
    void setRelativeHumidity(int value) { relativeHumidity = value; }
    void setDewPoint(float value) { dewPoint = value; }
    void setWindChill(float value) { windChill = value; }
    void setApparentTemperature(float value) { apparentTemperature = value; }
    void setAbsolutePressure(float value) { absolutePressure = value; }
    void setAverageWindSpeed(float value) { averageWindSpeed = value; }
    void setGustWindSpeed(float value) { gustWindSpeed = value; }
    void setWindDirection(QString value) { windDirection = value; }
    void setTimestamp(QDateTime value) { timestamp = value; }
    void setIndoorDataAvailable(bool available) { _indoorDataAvailable = available;}

private:
    float indoorTemperature;
    int indoorRelativeHumidity;
    float temperature;
    int relativeHumidity;
    float dewPoint;
    float windChill;
    float apparentTemperature;
    float absolutePressure;
    float averageWindSpeed;
    float gustWindSpeed;
    QString windDirection;
    QDateTime timestamp;
    bool _indoorDataAvailable;
};

/** Provides access to data from the zxweather system.
 *
 */
class AbstractDataSource : public QObject
{
    Q_OBJECT
public:
    explicit AbstractDataSource(QObject *parent = 0);
    
    /** Provides access to live data.
     * @return A new live data object.
     */
    virtual AbstractLiveData* getLiveData() = 0;

    /** Returns true if the data source is connected */
    virtual bool isConnected() {return true;}

signals:
    /** Emitted when ever new live data is available.
     */
    void liveDataRefreshed();
};

#endif // DATASOURCE_H
