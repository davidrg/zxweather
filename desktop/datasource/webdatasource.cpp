#include "webdatasource.h"
#include "constants.h"
#include "settings.h"

#include <QMessageBox>
#include <QStringList>
#include <QNetworkRequest>
#include <QNetworkDiskCache>
#include <QtDebug>
#include <float.h>


#define SET_MIN(field) if (temp < field) field = temp;
#define SET_MAX(field) if (temp > field) field = temp;

WebDataSource::WebDataSource(QString baseURL, QWidget *parentWidget, QObject *parent) :
    AbstractDataSource(parent)
{
    this->baseURL = baseURL;
    this->parentWidget = parentWidget;

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

        if (parts.count() < 10) continue; // invalid record.

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
    }

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


void WebDataSource::dataReady(QNetworkReply* reply) {
    qDebug() << "Data ready";

    if (reply->error() != QNetworkReply::NoError) {
        QMessageBox::warning(NULL, "Download failed", reply->errorString());
        progressDialog->reset();
        dataSetQueue.clear();
        urlQueue.clear();
    } else if (progressDialog->wasCanceled()) {
        progressDialog->reset();
        dataSetQueue.clear();
        urlQueue.clear();
    } else {

        while(!reply->atEnd()) {
            QString line = reply->readLine();
            if (!line.startsWith("#"))
                allData.append(line.trimmed());
        }

        progressDialog->setValue(progressDialog->value() + 1);
        if(urlQueue.isEmpty()) {
            // Finished downloading everything.
            processData();
        } else {
            // Still more to download.
            downloadNextDataSet();
        }
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

    progressDialog.reset(new QProgressDialog(parentWidget));
    progressDialog->setWindowTitle("Downloading data sets...");

    urlQueue = getURLList(baseURL, startTime, endTime, &dataSetQueue);
    progressDialog->setValue(0);
    progressDialog->setRange(0, urlQueue.count() +1);
    progressDialog->show();

    downloadNextDataSet();
}
