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

QStringList getURLList(
        QString baseURL, QDateTime startTime, QDateTime endTime,
        QStringList* dataSetQueue);

WebDataSource::WebDataSource(QWidget *parentWidget, QObject *parent) :
    AbstractDataSource(parentWidget, parent)
{
    Settings& settings = Settings::getInstance();
    baseURL = settings.webInterfaceUrl();
    stationCode = settings.stationCode();
    liveDataUrl = QUrl(baseURL + "data/" + stationCode + "/live.json");

    rangeRequest = false;

    netAccessManager.reset(new QNetworkAccessManager(this));
    connect(netAccessManager.data(), SIGNAL(finished(QNetworkReply*)),
            this, SLOT(dataReady(QNetworkReply*)));

    // Setup data cache thing.
    QNetworkDiskCache* cache = new QNetworkDiskCache(this);
    cache->setCacheDirectory(Settings::getInstance().dataSetCacheDir());
    netAccessManager->setCache(cache);


    // Setup live data functionality
    liveNetAccessManager.reset(new QNetworkAccessManager(this));
    connect(liveNetAccessManager.data(), SIGNAL(finished(QNetworkReply*)),
            this, SLOT(liveDataReady(QNetworkReply*)));
    livePollTimer.setInterval(30000);
    connect(&livePollTimer, SIGNAL(timeout()), this, SLOT(liveDataPoll()));
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

    while (!timeStamps.isEmpty()) {
        samples.timestamp.append(timeStamps.takeFirst().toTime_t());

        QStringList values = sampleParts.takeFirst();

        samples.temperature.append(values.takeFirst().toDouble());
        samples.dewPoint.append(values.takeFirst().toDouble());
        samples.apparentTemperature.append(values.takeFirst().toDouble());
        samples.windChill.append(values.takeFirst().toDouble());
        samples.humidity.append(values.takeFirst().toDouble());
        samples.pressure.append(values.takeFirst().toDouble());
        samples.indoorTemperature.append(values.takeFirst().toDouble());
        samples.indoorHumidity.append(values.takeFirst().toDouble());
        samples.rainfall.append(values.takeFirst().toDouble());
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

//--------------------------------------------------------------------------

void WebDataSource::liveDataReady(QNetworkReply *reply) {
    using namespace QtJson;

    if (reply->error() != QNetworkReply::NoError) {
        emit error(reply->errorString());
    } else {
        LiveDataSet lds;
        bool ok;

        QVariantMap result = Json::parse(reply->readAll(), ok).toMap();

        if (!ok) {
            emit error("JSON parsing failed");
            return;
        }

        lds.windDirection = result["wind_direction"].toFloat();
        lds.windSpeed = result["average_wind_speed"].toFloat();
        lds.temperature = result["temperature"].toFloat();
        lds.dewPoint = result["dew_point"].toFloat();
        lds.windChill = result["wind_chill"].toFloat();
        lds.gustWindSpeed = result["gust_wind_speed"].toFloat();
        lds.humidity = result["relative_humidity"].toInt();
        lds.timestamp = QDateTime::fromString(
                             result["time_stamp"].toString(), "HH:mm:ss");
        lds.apparentTemperature = result["apparent_temperature"].toFloat();
        lds.pressure = result["absolute_pressure"].toFloat();

        // Indoor data is not currently available from the website data feed.
        lds.indoorDataAvailable = false;

        emit liveData(lds);
    }
}

void WebDataSource::liveDataPoll() {
    QNetworkRequest request;
    request.setUrl(liveDataUrl);
    request.setRawHeader("User-Agent", Constants::USER_AGENT);

    liveNetAccessManager->get(request);
}

void WebDataSource::enableLiveData() {
    liveDataPoll();
    livePollTimer.start();
}
