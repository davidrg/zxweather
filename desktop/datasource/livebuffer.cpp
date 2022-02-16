#include "livebuffer.h"
#include "settings.h"

#include <QtDebug>
#include <QDir>
#include <QFile>
#include <QString>
#include <QStringList>

#if QT_VERSION >= 0x050000
#include <QStandardPaths>
#else
#include <QDesktopServices>
#endif


LiveBuffer::LiveBuffer()
{
    maxHours = Settings::getInstance().liveBufferHours();
    load();
}

LiveBuffer::~LiveBuffer()
{
    save();
}

void LiveBuffer::connectStation(QString station) {
    save();
    if (station != stationCode) {
        buffer.clear();
    }
    stationCode = station;
    load();
}

void LiveBuffer::liveData(LiveDataSet data) {
    buffer.append(data);

    trimBuffer();

    if (!lastSaveTime.isValid() || lastSaveTime < QDateTime::currentDateTime().addSecs(-300)) {
        // Append last 5 minutes of data to archive file.

        bool append = true;

        if (lastFileOverwriteTime < QDateTime::currentDateTime().addSecs(0 - maxHours * 60 * 60) || !lastFileOverwriteTime.isValid()) {
            append = false; // Last time we rewrote the file was maxHours or more ago. Trash the
                            // current contents and rewrite it to stop the file growing too large.
        }

        qDebug() << "Live buffer:" << buffer.length();

        save(append);
    }

    //qDebug() << "Now have" << buffer.length() << "records in memory.";
}

void LiveBuffer::trimBuffer() {
    if (buffer.isEmpty()) {
        return;
    }

    int keepSecs = maxHours * 60 * 60;

    QDateTime minTime = QDateTime::currentDateTime().addSecs(0-keepSecs);

    //int count = buffer.length();
    while(!buffer.isEmpty() && buffer.at(0).timestamp < minTime) {
        buffer.removeAt(0);
    }

    //qDebug() << "Removed" << count - buffer.length() << "live records from cache";
}

QList<LiveDataSet> LiveBuffer::getData() {
    return buffer;
}

QString LiveBuffer::encodeLiveDataSet(LiveDataSet lds) {
    QString result = QString("%1\t%2\t%3\t%4\t%5\t%6\t%7\t%8\t%9\t%10\t"
                             "%11\t%12\t%13\t%14\t%15\t%16\t%17\t%18\t%19\t%20\t"
                             "%21\t%22\t%23\t%24\t%25\t%26\t%27\t%28\t%29\t%30\t"
                             "%31\t%32\t%33\t%34\t%35\t%36\t%37\t%38\t%39\t%40\t"
                             "%41\n")
            .arg(lds.timestamp.toString(Qt::ISODate))
            .arg(lds.hw_type)
            .arg(lds.indoorDataAvailable ? "t" : "f")
            .arg(lds.temperature, 0, 'f', 1)
            .arg(lds.indoorTemperature, 0, 'f', 1)
            .arg(lds.apparentTemperature, 0, 'f', 1)
            .arg(lds.windChill, 0, 'f', 1)
            .arg(lds.dewPoint, 0, 'f', 1)
            .arg(lds.humidity)
            .arg(lds.indoorHumidity)
            .arg(lds.pressure)
            .arg(lds.windSpeed, 0, 'f', 1)
            .arg(lds.windDirection)
            .arg(lds.davisHw.stormRain)
            .arg(lds.davisHw.rainRate)
            .arg(lds.davisHw.stormStartDate.toString(Qt::ISODate))
            .arg(lds.davisHw.stormDateValid ? "t" : "f")
            .arg(lds.davisHw.barometerTrend)
            .arg(lds.davisHw.forecastIcon)
            .arg(lds.davisHw.forecastRule)
            .arg(lds.davisHw.txBatteryStatus)
            .arg(lds.davisHw.consoleBatteryVoltage, 0, 'f', 1)
            .arg(lds.davisHw.uvIndex)
            .arg(lds.davisHw.solarRadiation)
            .arg(lds.davisHw.leafWetness1)
            .arg(lds.davisHw.leafWetness2)
            .arg(lds.davisHw.leafTemperature1, 0, 'f', 1)
            .arg(lds.davisHw.leafTemperature2, 0, 'f', 1)
            .arg(lds.davisHw.soilMoisture1)
            .arg(lds.davisHw.soilMoisture2)
            .arg(lds.davisHw.soilMoisture3)
            .arg(lds.davisHw.soilMoisture4)
            .arg(lds.davisHw.soilTemperature1, 0, 'f', 1)
            .arg(lds.davisHw.soilTemperature2, 0, 'f', 1)
            .arg(lds.davisHw.soilTemperature3, 0, 'f', 1)
            .arg(lds.davisHw.soilTemperature4, 0, 'f', 1)
            .arg(lds.davisHw.extraTemperature1, 0, 'f', 1)
            .arg(lds.davisHw.extraTemperature2, 0, 'f', 1)
            .arg(lds.davisHw.extraTemperature3, 0, 'f', 1)
            .arg(lds.davisHw.extraHumidity1)
            .arg(lds.davisHw.extraHumidity2);
    return result;
}

LiveDataSet LiveBuffer::decodeLiveDataSet(QString row) {
    QStringList parts = row.trimmed().split("\t");

    LiveDataSet result;

//    qDebug() << parts.length() << parts;

    if (parts.length() < 41) {
        qDebug() << "invalid row";
        return result; // Invalid row.
    }

    result.timestamp = QDateTime::fromString(parts.at(0), Qt::ISODate);
    result.hw_type = (hardware_type_t)parts.at(1).toInt();
    result.indoorDataAvailable = parts.at(2) == "t";
    result.temperature = parts.at(3).toFloat();
    result.indoorTemperature = parts.at(4).toFloat();
    result.apparentTemperature = parts.at(5).toFloat();

    result.windChill = parts.at(6).toFloat();
    result.dewPoint = parts.at(7).toFloat();
    result.humidity = parts.at(8).toInt();
    result.indoorHumidity = parts.at(9).toInt();
    result.pressure = parts.at(10).toFloat();
    result.windSpeed = parts.at(11).toFloat();
    result.windDirection = parts.at(12).toInt();
    result.davisHw.stormRain = parts.at(13).toFloat();
    result.davisHw.rainRate = parts.at(14).toFloat();
    result.davisHw.stormStartDate = QDateTime::fromString(parts.at(15), Qt::ISODate).date();
    result.davisHw.stormDateValid = parts.at(16) == "t";
    result.davisHw.barometerTrend = parts.at(17).toInt();
    result.davisHw.forecastIcon = parts.at(18).toInt();
    result.davisHw.forecastRule = parts.at(19).toInt();
    result.davisHw.txBatteryStatus = parts.at(20).toInt();
    result.davisHw.consoleBatteryVoltage = parts.at(21).toFloat();
    result.davisHw.uvIndex = parts.at(22).toFloat();
    result.davisHw.solarRadiation = parts.at(23).toFloat();

    result.davisHw.leafWetness1 = parts.at(24).toFloat();
    result.davisHw.leafWetness2 = parts.at(25).toFloat();
    result.davisHw.leafTemperature1 = parts.at(26).toFloat();
    result.davisHw.leafTemperature2 = parts.at(27).toFloat();
    result.davisHw.soilMoisture1 = parts.at(28).toFloat();
    result.davisHw.soilMoisture2 = parts.at(29).toFloat();
    result.davisHw.soilMoisture3 = parts.at(30).toFloat();
    result.davisHw.soilMoisture4 = parts.at(31).toFloat();
    result.davisHw.soilTemperature1 = parts.at(32).toFloat();
    result.davisHw.soilTemperature2 = parts.at(33).toFloat();
    result.davisHw.soilTemperature3 = parts.at(34).toFloat();
    result.davisHw.soilTemperature4 = parts.at(35).toFloat();
    result.davisHw.extraTemperature1 = parts.at(36).toFloat();
    result.davisHw.extraTemperature2 = parts.at(37).toFloat();
    result.davisHw.extraTemperature3 = parts.at(38).toFloat();
    result.davisHw.extraHumidity1 = parts.at(39).toFloat();
    result.davisHw.extraHumidity2 = parts.at(40).toFloat();

    return result;
}

QString LiveBuffer::bufferFilename() {
    if (stationCode.isNull() || stationCode.isEmpty()) {
        return QString();
    }

#if QT_VERSION >= 0x050000
    QString filename = QStandardPaths::writableLocation(
                QStandardPaths::CacheLocation);
#else
    QString filename = QDesktopServices::storageLocation(
                QDesktopServices::CacheLocation);
#endif

    filename += QDir::separator();
    filename += "live_buffer";
    filename += QDir::separator();

    if (!QDir(filename).exists())
        QDir().mkpath(filename);

    filename += stationCode + ".dat";

    return filename;
}

void LiveBuffer::save(bool appendOnly) {
    QString filename = bufferFilename();

    if (stationCode.isNull() || stationCode.isEmpty() || filename.isNull()) {
        return;
    }

    QFile f(filename);

    lastSaveTime = QDateTime::currentDateTime();

    if (appendOnly && f.exists() && lastFileWriteTime.isValid()) {
        qDebug() << "Appending live file";
        if (f.open(QFile::Append)) {
            foreach (LiveDataSet lds, buffer) {
                if (lds.timestamp > lastFileWriteTime) {
                    f.write(encodeLiveDataSet(lds).toLatin1());

                    if (lds.timestamp > lastFileWriteTime) {
                        lastFileWriteTime = lds.timestamp;
                    }
                }
            }
            f.flush();
            f.close();
            return;
        }
    }

    lastFileOverwriteTime = QDateTime::currentDateTime();
    lastFileWriteTime = QDateTime::currentDateTime();

    qDebug() << "rewriting live file";

    if (f.exists()) {
        f.remove();
    }

    if (f.open(QFile::WriteOnly)) {
        foreach (LiveDataSet lds, buffer) {
            f.write(encodeLiveDataSet(lds).toLatin1());
            if (lds.timestamp > lastFileWriteTime) {
                lastFileWriteTime = lds.timestamp;
            }
        }
        f.flush();
        f.close();
    }
}

bool ldsLessThan(const LiveDataSet &s1, const LiveDataSet &s2)
{
    return s1.timestamp < s2.timestamp;
}

void LiveBuffer::load() {
    QString filename = bufferFilename();

    if (stationCode.isNull() || stationCode.isEmpty() || filename.isNull()) {
        return;
    }

    buffer.clear();

    QFile f(filename);

    if (!f.exists()) {
        return;
    }

    if (f.open(QFile::ReadOnly)) {
        QString data(f.readAll());

        QStringList lines = data.split("\n");

        foreach (QString line, lines) {
            LiveDataSet lds = decodeLiveDataSet(line);
            if (lds.timestamp.isValid()) {
                buffer.append(lds);
            }

        }

        if (!buffer.isEmpty()) {
            std::sort(buffer.begin(), buffer.end(), ldsLessThan);
        }
    }

    trimBuffer();
}
