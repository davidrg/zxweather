#include "weatherimagewindow.h"
#include "ui_weatherimagewindow.h"
#include "settings.h"
#include "datasource/webdatasource.h"
#include "datasource/databasedatasource.h"
#include "datasource/dialogprogresslistener.h"
#include "json/json.h"
#include "constants.h"
#include "unit_conversions.h"
#include "charts/chartwindow.h"

#include <QtDebug>
#include <QList>

WeatherImageWindow::WeatherImageWindow(QWidget *parent) :
    QWidget(parent),
    ui(new Ui::WeatherImageWindow)
{
    ui->setupUi(this);
    ui->image->setScaled(true);
    Settings& settings = Settings::getInstance();

    if (settings.sampleDataSourceType() == Settings::DS_TYPE_DATABASE)
        dataSource.reset(new DatabaseDataSource(new DialogProgressListener(this), this));
    else
        dataSource.reset(new WebDataSource(new DialogProgressListener(this), this));

    connect(dataSource.data(), SIGNAL(imageReady(ImageInfo,QImage,QString)),
            this, SLOT(imageReady(ImageInfo,QImage,QString)));
    connect(dataSource.data(), SIGNAL(samplesReady(SampleSet)),
            this, SLOT(samplesReady(SampleSet)));
    connect(ui->image, SIGNAL(videoPositionChanged(qint64)),
            this, SLOT(mediaPositionChanged(qint64)));

    connect(ui->temperature, SIGNAL(plotRequested(DataSet)),
            this, SLOT(plotRequested(DataSet)));
    connect(ui->apparentTemperature, SIGNAL(plotRequested(DataSet)),
            this, SLOT(plotRequested(DataSet)));
    connect(ui->windChill, SIGNAL(plotRequested(DataSet)),
            this, SLOT(plotRequested(DataSet)));
    connect(ui->dewPoint, SIGNAL(plotRequested(DataSet)),
            this, SLOT(plotRequested(DataSet)));
    connect(ui->humidity, SIGNAL(plotRequested(DataSet)),
            this, SLOT(plotRequested(DataSet)));
    connect(ui->barometer, SIGNAL(plotRequested(DataSet)),
            this, SLOT(plotRequested(DataSet)));
    connect(ui->windSpeed, SIGNAL(plotRequested(DataSet)),
            this, SLOT(plotRequested(DataSet)));
    ui->windSpeed->setName("ldw_wind_speed");
    connect(ui->windDirection, SIGNAL(plotRequested(DataSet)),
            this, SLOT(plotRequested(DataSet)));
    connect(ui->uvIndex, SIGNAL(plotRequested(DataSet)),
            this, SLOT(plotRequested(DataSet)));
    connect(ui->solarRadiation, SIGNAL(plotRequested(DataSet)),
            this, SLOT(plotRequested(DataSet)));
    connect(ui->rain, SIGNAL(plotRequested(DataSet)),
            this, SLOT(plotRequested(DataSet)));
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
        if (imageInfo.mimeType.startsWith("video/"))
            setWindowIcon(QIcon(":/icons/film"));
        else if (imageInfo.mimeType.startsWith("audio/"))
            setWindowIcon(QIcon(":/icons/audio"));
    }

    // TODO: confirm all of these are required.
    QDateTime start;
    QDateTime finish;
    bool haveInterval = false;
    uint interval = 300;
    bool haveFrameCount = false;
    uint frameCount = 1;

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
        videoSync = false;
        if (!isImage) {
            if (imageInfo.mimeType.startsWith("video/")) {
                ui->message->setText(tr("Insufficient video metadata"));
            } else {
                ui->message->setText(tr("Insufficient metadata"));
            }
        }

        dataSource.data()->fetchSamples(ALL_SAMPLE_COLUMNS,
                                        imageInfo.timeStamp.addSecs(-600),
                                        imageInfo.timeStamp);
    } else {
        // A video (or audio file) with metadata!

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

        videoStart = start;
        videoEnd = finish;
        frameInterval = interval;
        this->frameCount = frameCount;
        videoSync = true;

        dataSource.data()->fetchSamples(ALL_SAMPLE_COLUMNS,
                                        start.addSecs(-600),
                                        finish);
    }
}

QString tempString(double temp) {
    QString formatString = "%1" TEMPERATURE_SYMBOL;
    return QString(formatString).arg(QString::number(temp, 'f', 1));
}

void WeatherImageWindow::samplesReady(SampleSet samples) {
    if (!videoSync) {
        // An image or a video with insufficient metadata
        uint maxTs = 0;
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
    using namespace UnitConversions;

    QDateTime dateTime = QDateTime::fromTime_t(samples.timestampUnix[i]);
    ui->date->setText(dateTime.date().toString());
    ui->time->setText(dateTime.time().toString());

    UnitValue temperature = samples.temperature[i];
    temperature.unit = U_CELSIUS;

    UnitValue indoorTemperature = samples.indoorTemperature[i];
    indoorTemperature.unit = U_CELSIUS;

    ui->temperature->setOutdoorIndoorValue(temperature, SC_Temperature,
                                           indoorTemperature, SC_IndoorTemperature);

    UnitValue apparentTemperature = samples.apparentTemperature[i];
    apparentTemperature.unit = U_CELSIUS;
    ui->apparentTemperature->setValue(apparentTemperature, SC_ApparentTemperature);

    UnitValue windChill = samples.windChill[i];
    windChill.unit = U_CELSIUS;
    ui->windChill->setValue(windChill, SC_WindChill);

    UnitValue dewPoint = samples.dewPoint[i];
    dewPoint.unit = U_CELSIUS;
    ui->dewPoint->setValue(dewPoint, SC_DewPoint);

    UnitValue humidity = samples.humidity[i];
    humidity.unit = U_HUMIDITY;

    UnitValue indoorHumidiy = samples.indoorHumidity[i];
    indoorHumidiy.unit = U_HUMIDITY;

    ui->humidity->setOutdoorIndoorValue(humidity, SC_Humidity,
                                        indoorHumidiy, SC_IndoorHumidity);

    UnitValue barometer = samples.pressure[i];
    barometer.unit = U_HECTOPASCALS;
    ui->barometer->setValue(barometer, SC_Pressure);

    UnitValue windSpeed = samples.averageWindSpeed[i];
    windSpeed.unit = U_METERS_PER_SECOND;

    UnitValue bft = metersPerSecondtoBFT(windSpeed);
    bft.unit = U_BFT;

    ui->windSpeed->setDoubleValue(windSpeed, SC_AverageWindSpeed,
                                  bft, SC_AverageWindSpeed);

    uint dirIdx = samples.timestampUnix[i];

    if (samples.windDirection.contains(dirIdx)) {
        int direction = samples.windDirection[dirIdx];

        UnitValue windDirections = direction;
        windDirections.unit = U_DEGREES;

        UnitValue compassPoint = direction;
        compassPoint.unit = U_COMPASS_POINT;

        ui->windDirection->setDoubleValue(windDirections, SC_WindDirection,
                                          compassPoint, SC_WindDirection);
    } else {
        ui->windDirection->clear();
    }

    if (samples.uvIndex.isEmpty()) {
        ui->uvIndex->clear();
    } else {
        UnitValue uvIndex = samples.uvIndex[i];
        uvIndex.unit = U_UV_INDEX;

        ui->uvIndex->setValue(uvIndex, SC_UV_Index);
    }

    if (samples.solarRadiation.isEmpty()) {
        ui->solarRadiation->clear();
    } else {
        UnitValue solarRadiation = samples.solarRadiation[i];
        solarRadiation.unit = U_WATTS_PER_SQUARE_METER;

        ui->solarRadiation->setValue(solarRadiation, SC_SolarRadiation);
    }

    rainTotal += samples.rainfall[i];

    UnitValue rain = rainTotal;
    rain.unit = U_MILLIMETERS;

    ui->rain->setValue(rain, SC_Rainfall);
}

void WeatherImageWindow::plotRequested(DataSet ds) {
    QList<DataSet> datasets;
    datasets << ds;

    qDebug() << "DS Columns:"<< (int)ds.columns;
    qDebug() << "Start" << ds.startTime;
    qDebug() << "End" << ds.endTime;
    qDebug() << "AGFunc" << ds.aggregateFunction;
    qDebug() << "AGGrp" << ds.groupType;
    qDebug() << "AGMin" << ds.customGroupMinutes;


    station_info_t info = dataSource->getStationInfo();
    bool wirelessAvailable = false;
    bool solarAvailable = false;
    if (info.isValid) {
        wirelessAvailable = info.isWireless;
        solarAvailable = info.hasSolarAndUV;
    }

    ChartWindow *cw = new ChartWindow(datasets, solarAvailable, wirelessAvailable);
    cw->setAttribute(Qt::WA_DeleteOnClose);
    cw->show();
}
