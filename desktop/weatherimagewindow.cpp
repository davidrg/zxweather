#include "weatherimagewindow.h"
#include "ui_weatherimagewindow.h"
#include "settings.h"
#include "datasource/webdatasource.h"
#include "datasource/databasedatasource.h"
#include "json/json.h"
#include "constants.h"

#include <QtDebug>

WeatherImageWindow::WeatherImageWindow(QWidget *parent) :
    QWidget(parent),
    ui(new Ui::WeatherImageWindow)
{
    ui->setupUi(this);
    ui->image->setScaled(true);
    Settings& settings = Settings::getInstance();

    if (settings.sampleDataSourceType() == Settings::DS_TYPE_DATABASE)
        dataSource.reset(new DatabaseDataSource(this, this));
    else
        dataSource.reset(new WebDataSource(this, this));

    connect(dataSource.data(), SIGNAL(imageReady(ImageInfo,QImage,QString)),
            this, SLOT(imageReady(ImageInfo,QImage,QString)));
    connect(dataSource.data(), SIGNAL(samplesReady(SampleSet)),
            this, SLOT(samplesReady(SampleSet)));
    connect(ui->image, SIGNAL(videoPositionChanged(qint64)),
            this, SLOT(mediaPositionChanged(qint64)));

    windDirections << "N" << "NNE" << "NE" << "ENE" << "E" << "ESE" << "SE"
                   << "SSE" << "S" << "SSW" << "SW" << "WSW" << "W" << "WNW"
                   << "NW" << "NNW";
}

WeatherImageWindow::~WeatherImageWindow()
{
    delete ui;
}

void WeatherImageWindow::setImage(int imageId) {
    dataSource.data()->fetchImage(imageId);
    rainTotal = 0;
}

void WeatherImageWindow::imageReady(ImageInfo imageInfo, QImage image, QString filename) {
    ui->image->setImage(image, imageInfo, filename);
    setWindowTitle(imageInfo.title);
    isImage = imageInfo.mimeType.startsWith("image/");

    if (!isImage) {
        setWindowIcon(QIcon(":/icons/film"));
    }

    // TODO: confirm all of these are required.
    QDateTime start;
    QDateTime finish;
    bool haveInterval = false;
    uint interval;
    bool haveFrameCount = false;
    uint frameCount;

    if (!isImage && imageInfo.hasMetadata && !imageInfo.metadata.isNull()) {
        using namespace QtJson;

        bool ok;
        QVariant doc = Json::parse(imageInfo.metadata, ok);
        if (doc.canConvert<QVariantMap>()) {
            QVariantMap map = doc.toMap();
            if (map.contains("start")) {
                start = map["start"].toDateTime();
            }
            if (map.contains("finish")) {
                finish = map["finish"].toDateTime();
            }
            if (map.contains("interval")) {
                interval = map["interval"].toUInt(&haveInterval);
            }
            if (map.contains("frame_count")) {
                frameCount = map["frame_count"].toUInt(&haveFrameCount);
            }
            if (map.contains("frame_rate")) {
                bool frOk;
                framesPerSecond = map["frame_rate"].toUInt(&frOk);
                if (!frOk) {
                    framesPerSecond = 30;
                }
            } else {
                framesPerSecond = 30;
            }
        }
    }

    bool haveVideoMeta = !start.isNull() &&
            !finish.isNull() && haveInterval &&
            haveFrameCount;

    // If its an image or a video without metadata, show metadata for the
    // timestamp. For a video this will probably be as of the end of the video.
    if (isImage || (!isImage && !haveVideoMeta)) {
        dataSource.data()->fetchSamples(ALL_SAMPLE_COLUMNS,
                                        imageInfo.timeStamp.addSecs(-600),
                                        imageInfo.timeStamp);
        videoSync = false;
        if (!isImage) {
            ui->message->setText(tr("Insufficient video metadata"));
        }
    } else {
        // A video with metadata!

        // Lock the controls so the user can't start playback until we've
        // fetched the weather data.
        ui->image->setVideoControlsLocked(true);
        ui->image->setVideoControlsEnabled(false);

        // Number of real time seconds in a second of video. With a frame
        // interval of 60 (one frame captured every minute) and a framerate of
        // 30 each second of video would cover 30 minutes of real time.
        int secondsPerVideoSecond = interval * framesPerSecond;

        // Assuming the sample interval is 5 minutes there will be this many
        // samples per second of video:
        double samplesPerSecond = (secondsPerVideoSecond / 60.0) / 5.0;
        // (minutes per video second / 5 minutes)

        // TODO: Get sample interval from station config above. That or just
        // have a look at a few samples and see what the gap between their
        // timestamps is.

        // Interval (in milliseconds) between new samples. This is how often we
        // want to be notified about video playback position.
        double sampleInterval = 1000 / samplesPerSecond;

        ui->image->setVideoTickInterval(sampleInterval);
        msPerSample = sampleInterval;

        dataSource.data()->fetchSamples(ALL_SAMPLE_COLUMNS,
                                        start.addSecs(-600),
                                        finish);
        videoStart = start;
        videoEnd = finish;
        frameInterval = interval;
        this->frameCount = frameCount;
        videoSync = true;
    }
}

QString tempString(double temp) {
    QString formatString = "%1" TEMPERATURE_SYMBOL;
    return QString(formatString).arg(QString::number(temp, 'f', 1));
}

void WeatherImageWindow::samplesReady(SampleSet samples) {
    if (!videoSync) {
        // An image or a video with insufficient metadata
        uint maxTs;
        foreach (uint ts, samples.timestampUnix) {
            maxTs = qMax(maxTs, ts);
        }

        int i = samples.timestampUnix.indexOf(maxTs);

        displaySample(samples, i);
    } else {
        // A video with enough metadata to sync up samples during playback
        videoSamples = samples;
    }

    ui->image->setVideoControlsLocked(false);
    ui->image->setVideoControlsEnabled(true);
}

void WeatherImageWindow::mediaPositionChanged(qint64 time) {
    // If we didn't have enough metadata for the video don't bother looking up
    // samples for the video as it plays
    if (!videoSync) return;

    // time is in milliseconds
    qint64 seconds = (time / msPerSample) * (5*60); //(time / 1000) * frameInterval * 30;
    qDebug() << "seconds: " << seconds
             << "time: " << time
             << "time/1000:" << time/1000
             << "frameInterval: " << frameInterval;

    //

    QDateTime sampleTime = videoStart.addSecs(seconds);
    qDebug() << sampleTime;
    int index = 0;

    for (int i = 0;  i < videoSamples.timestampUnix.count(); i++) {
        QDateTime t = QDateTime::fromTime_t(videoSamples.timestampUnix[i]);
        if (t > sampleTime) {
            // The previous sample is the correct one for this time.
            index = i - 1;
            if (index >= 0) {
                displaySample(videoSamples, index);
            }
            return;
        }
    }
}

void WeatherImageWindow::displaySample(SampleSet samples, int i) {
    QDateTime dateTime = QDateTime::fromTime_t(samples.timestampUnix[i]);
    ui->date->setText(dateTime.date().toString());
    ui->time->setText(dateTime.time().toString());
    QString formatString = "%1 (%2 inside)";
    ui->temperature->setText(formatString
            .arg(tempString(samples.temperature[i]))
            .arg(tempString(samples.indoorTemperature[i])));
    ui->apparentTemperature->setText(tempString(samples.apparentTemperature[i]));
    ui->windChill->setText(tempString(samples.windChill[i]));
    ui->dewPoint->setText(tempString(samples.dewPoint[i]));
    ui->humidity->setText(QString("%1% (%2% inside)").arg(
                              samples.humidity[i]).arg(samples.indoorHumidity[i]));
    ui->barometer->setText(QString("%1 hPa").arg(samples.pressure[i]));

    double windSpeed = samples.averageWindSpeed[i];

    QString bft = "";
    if (windSpeed < 0.3) // 0
        bft = "calm";
    else if (windSpeed < 2) // 1
        bft = "light air";
    else if (windSpeed < 3) // 2
        bft = "light breeze";
    else if (windSpeed < 5.4) // 3
        bft = "gentle breeze";
    else if (windSpeed < 8) // 4
        bft = "moderate breeze";
    else if (windSpeed < 10.7) // 5
        bft = "fresh breeze";
    else if (windSpeed < 13.8) // 6
        bft = "strong breeze";
    else if (windSpeed < 17.1) // 7
        bft = "high wind, near gale";
    else if (windSpeed < 20.6) // 8
        bft = "gale, fresh gale";
    else if (windSpeed < 24.4) // 9
        bft = "strong gale";
    else if (windSpeed < 28.3) // 10
        bft = "storm, whole gale";
    else if (windSpeed < 32.5) // 11
        bft = "violent storm";
    else // 12
        bft = "hurricane";

    if (!bft.isEmpty())
        bft = " (" + bft + ")";

    ui->windSpeed->setText(
                QString::number(windSpeed, 'f', 1) + " m/s" + bft);

    uint dirIdx = samples.timestampUnix[i];

    if (samples.windDirection.contains(dirIdx)) {
        uint direction = samples.windDirection[dirIdx];

        int idx = (((direction * 100) + 1125) % 36000) / 2250;
        QString point = windDirections.at(idx);

        ui->windDirection->setText(QString("%1" DEGREE_SYMBOL " %2").arg(
                                       direction).arg(point));
    } else {
        ui->windDirection->setText("--");
    }

    ui->uvIndex->setText(QString::number(samples.uvIndex[i], 'f', 1));
    ui->solarRadiation->setText(QString::number(samples.solarRadiation[i])
                                   + " W/m" SQUARED_SYMBOL);

    rainTotal += samples.rainfall[i];
    ui->rain->setText(QString::number(rainTotal, 'f', 1) + " mm");
}
