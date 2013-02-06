#include "webdatasource.h"
#include "constants.h"
#include "settings.h"
#include "json/json.h"

#include <QMessageBox>
#include <QStringList>
#include <QNetworkRequest>
#include <QNetworkDiskCache>
#include <QVariantMap>
#include <QtDebug>
#include <float.h>

// TODO: make the progress dialog cancel button work.


#define SET_MIN(field) if (temp < field) field = temp;
#define SET_MAX(field) if (temp > field) field = temp;


QStringList getURLList(
        QString baseURL, QDateTime startTime, QDateTime endTime,
        QStringList* dataSetQueue);

WebDataSource::WebDataSource(QString baseURL, QString stationCode, QWidget *parentWidget, QObject *parent) :
    AbstractDataSource(parentWidget, parent)
{
    this->baseURL = baseURL;
    this->stationCode = stationCode;

    rangeRequest = false;

    netAccessManager.reset(new QNetworkAccessManager(this));
    connect(netAccessManager.data(), SIGNAL(finished(QNetworkReply*)),
            this, SLOT(dataReady(QNetworkReply*)));

    // Setup data cache thing.
    QNetworkDiskCache* cache = new QNetworkDiskCache(this);
    cache->setCacheDirectory(Settings::getInstance().dataSetCacheDir());
    netAccessManager->setCache(cache);
}

void WebDataSource::processData() {

    progressDialog->setLabelText("Processing...");

    SampleSet samples;

    // Split & trim the data
    QList<QDateTime> timeStamps;
    QList<QStringList> sampleParts;
    while (!allData.isEmpty()) {
        QString line = allData.takeFirst();
        QStringList parts = line.split(QRegExp("\\s+"));

        if (parts.count() < 11) continue; // invalid record.

        // Build timestamp
        QString tsString = parts.takeFirst();
        tsString += " " + parts.takeFirst();
        QDateTime timestamp = QDateTime::fromString(tsString, Qt::ISODate);

        if (timestamp >= start && timestamp <= end) {
            sampleParts.append(parts);
            timeStamps.append(timestamp);
        }
    }

    // Allocate memory for the sample set
    int size = timeStamps.count();
    samples.sampleCount = size;
    samples.timestamp.reserve(size);
    samples.temperature.reserve(size);
    samples.dewPoint.reserve(size);
    samples.apparentTemperature.reserve(size);
    samples.windChill.reserve(size);
    samples.indoorTemperature.reserve(size);
    samples.humidity.reserve(size);
    samples.indoorHumidity.reserve(size);
    samples.pressure.reserve(size);
    samples.rainfall.reserve(size);

    samples.minTimeStamp = DBL_MAX;
    samples.maxTimeStamp = DBL_MIN;
    samples.minTemperature = DBL_MAX;
    samples.maxTemperature = DBL_MIN;
    samples.minDewPoint = DBL_MAX;
    samples.maxDewPoint = DBL_MIN;
    samples.minApparentTemperature = DBL_MAX;
    samples.maxApparentTemperature = DBL_MIN;
    samples.minWindChill = DBL_MAX;
    samples.maxWindChill = DBL_MIN;
    samples.minIndoorTemperature = DBL_MAX;
    samples.maxIndoorTemperature = DBL_MIN;
    samples.minHumidity = DBL_MAX;
    samples.maxHumiditiy = DBL_MIN;
    samples.minIndoorHumidity = DBL_MAX;
    samples.maxIndoorHumidity = DBL_MIN;
    samples.minPressure = DBL_MAX;
    samples.maxPressure = DBL_MIN;
    samples.minRainfall = DBL_MAX;
    samples.maxRainfall = DBL_MIN;

    while (!timeStamps.isEmpty()) {
        double temp;

        temp = timeStamps.takeFirst().toTime_t();
        samples.timestamp.append(temp);
        SET_MIN(samples.minTimeStamp);
        SET_MAX(samples.maxTimeStamp);

        QStringList values = sampleParts.takeFirst();

        temp = values.takeFirst().toDouble();
        samples.temperature.append(temp);
        SET_MIN(samples.minTemperature);
        SET_MAX(samples.maxTemperature);

        temp = values.takeFirst().toDouble();
        samples.dewPoint.append(temp);
        SET_MIN(samples.minDewPoint);
        SET_MAX(samples.maxDewPoint);

        temp = values.takeFirst().toDouble();
        samples.apparentTemperature.append(temp);
        SET_MIN(samples.minApparentTemperature);
        SET_MAX(samples.maxApparentTemperature);

        temp = values.takeFirst().toDouble();
        samples.windChill.append(temp);
        SET_MIN(samples.minWindChill);
        SET_MAX(samples.maxWindChill);

        temp = values.takeFirst().toDouble();
        samples.humidity.append(temp);
        SET_MIN(samples.minHumidity);
        SET_MAX(samples.maxHumiditiy);

        temp = values.takeFirst().toDouble();
        samples.pressure.append(temp);
        SET_MIN(samples.minPressure);
        SET_MAX(samples.maxPressure);

        temp = values.takeFirst().toDouble();
        samples.indoorTemperature.append(temp);
        SET_MIN(samples.minIndoorTemperature);
        SET_MAX(samples.maxIndoorTemperature);

        temp = values.takeFirst().toDouble();
        samples.indoorHumidity.append(temp);
        SET_MIN(samples.minIndoorHumidity);
        SET_MAX(samples.maxIndoorHumidity);

        temp = values.takeFirst().toDouble();
        samples.rainfall.append(temp);
        SET_MIN(samples.minRainfall);
        SET_MAX(samples.maxRainfall)
    }

    progressDialog->setValue(progressDialog->value() + 1);
    progressDialog->setLabelText("Draw...");


    if (!failedDataSets.isEmpty())
        QMessageBox::warning(0,
            "Data sets failed to download",
            "The following data sets failed to download:\n" + failedDataSets.join("\n"));


    emit samplesReady(samples);
    progressDialog->reset();
}

void WebDataSource::downloadNextDataSet() {

    QString dataSetName = dataSetQueue.takeFirst();
    QString dataSetURL = urlQueue.takeFirst();

    qDebug() << "DataSet URL: " << dataSetURL;

    progressDialog->setLabelText(dataSetName + "...");

    QNetworkRequest request;
    request.setUrl(QUrl(dataSetURL));
    request.setRawHeader("User-Agent", Constants::USER_AGENT);

    netAccessManager->get(request);
}

void WebDataSource::rangeRequestResult(QString data) {
    rangeRequest = false;
    using namespace QtJson;

    bool ok;

    QVariantMap result = Json::parse(data, ok).toMap();

    if (!ok) {
        QMessageBox::warning(0, "Error","JSON parsing failed for timestamp range request");
        return;
    }

    minTimestamp = QDateTime::fromString(result["oldest"].toString(), Qt::ISODate);
    maxTimestamp = QDateTime::fromString(result["latest"].toString(), Qt::ISODate);

    // Clip request range if it falls outside the valid range.
    if (start < minTimestamp) start = minTimestamp;
    if (end > maxTimestamp) end = maxTimestamp;

    urlQueue = getURLList(baseURL + "b/" + stationCode + "/", start, end, &dataSetQueue);
    progressDialog->setValue(0);
    progressDialog->setRange(0, urlQueue.count() +1);

    downloadNextDataSet();
}

void WebDataSource::dataReady(QNetworkReply* reply) {
    qDebug() << "Data ready";

    if (reply->error() != QNetworkReply::NoError) {

        if (reply->error() != QNetworkReply::ContentNotFoundError || rangeRequest) {
            QMessageBox::warning(NULL, "Download failed", reply->errorString());
            progressDialog->reset();
            dataSetQueue.clear();
            urlQueue.clear();
            return;
        } else {
            failedDataSets.append(reply->request().url().toString());
        }
    } else if (progressDialog->wasCanceled()) {
        progressDialog->reset();
        dataSetQueue.clear();
        urlQueue.clear();
        return;
    } else if (rangeRequest){
        // Range request response succeeded. Process it.
        rangeRequestResult(reply->readAll());
        return;
    } else {
        // Data response succeeded. Process the data.
        while(!reply->atEnd()) {
            QString line = reply->readLine();
            if (!line.startsWith("#"))
                allData.append(line.trimmed());
        }
    }

    // Request more data if there is any to request
    progressDialog->setValue(progressDialog->value() + 1);
    if(urlQueue.isEmpty()) {
        // Finished downloading everything.
        processData();
    } else {
        // Still more to download.
        downloadNextDataSet();
    }
}

QString monthToName(int month) {
    switch (month) {
    case 1: return "january";
    case 2: return "february";
    case 3: return "march";
    case 4: return "april";
    case 5: return "may";
    case 6: return "june";
    case 7: return "july";
    case 8: return "august";
    case 9: return "september";
    case 10: return "october";
    case 11: return "november";
    case 12: return "december";
    default: return "";
    }
}

QStringList getURLList(
        QString baseURL, QDateTime startTime, QDateTime endTime,
        QStringList* dataSetQueue) {
    QDate startDate = startTime.date();
    QDate endDate = endTime.date();

    int startYear = startDate.year();
    int startMonth = startDate.month();

    int endYear = endDate.year();
    int endMonth = endDate.month();

    QStringList urlsToFetch;

    //TODO: consider trying to make use of day-level data sources if it makes
    // sense.

    for(int year = startYear; year <= endYear; year++) {
        qDebug() << "Year:" << year;
        int month = year == startYear ? startMonth : 1;
        for(; month <= 12; month++) {

            QString monthName = monthToName(month);

            QString url = baseURL + QString::number(year) + "/" +
                    monthName + "/gnuplot_data.dat";

            urlsToFetch.append(url);
            dataSetQueue->append(monthName + " " + QString::number(year));

            // We're finished.
            if (year == endYear && month == endMonth)
                break;
        }
    }
    return urlsToFetch;
}


void WebDataSource::fetchSamples(QDateTime startTime, QDateTime endTime) {
    start = startTime;
    end = endTime;

    progressDialog->setWindowTitle("Downloading data sets...");
    progressDialog->show();

    QNetworkRequest request;
    request.setUrl(QUrl(baseURL + "data/" + stationCode + "/samplerange.json"));
    request.setRawHeader("User-Agent", Constants::USER_AGENT);

    rangeRequest = true;
    netAccessManager->get(request);
}
