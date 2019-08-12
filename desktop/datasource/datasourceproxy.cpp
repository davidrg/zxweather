#include "datasourceproxy.h"

#include "webdatasource.h"
#include "databasedatasource.h"
#include "tcplivedatasource.h"

DataSourceProxy::DataSourceProxy(AbstractProgressListener* progressListener, QObject *parent)
    : AbstractDataSource(progressListener, parent)
{
    sampleSource = 0;
    liveSource = 0;
}

void DataSourceProxy::setDataSourceTypes(LiveDataSourceType live,
                                         DataSourceType data) {
    liveType = live;
    sampleType = data;
}

void DataSourceProxy::connectDataSources() {
    if (sampleSource == liveSource) {
        delete sampleSource;
        sampleSource = 0;
        liveSource = 0;
    } else {
        delete sampleSource;
        delete liveSource;
    }

    // Create sample data source
    if (sampleType == DST_DATABASE) {
        sampleSource = new DatabaseDataSource(progressListener, this);
    } else if (sampleType == DST_WEB) {
        sampleSource = new WebDataSource(progressListener, this);
    }

    // Create live data source
    if ((liveType == LDST_DATABASE && sampleType == DST_DATABASE) ||
            (liveType == LDST_WEB && sampleType == DST_WEB)) {
        liveSource = sampleSource;
    } else if (liveType == LDST_DATABASE) {
        liveSource = new DatabaseDataSource(progressListener, this);
    } else if (liveType == LDST_WEB) {
        liveSource = new DatabaseDataSource(progressListener, this);
    } else if (liveType == LDST_TCP) {
        liveSource = new TcpLiveDataSource(this);
    }

    // Hook up signals
    connect(liveSource, SIGNAL(liveData(LiveDataSet)),
            this, SLOT(liveDataSlot(LiveDataSet)));
    connect(liveSource, SIGNAL(error(QString)),
            this, SLOT(errorSlot(QString)));
    connect(liveSource, SIGNAL(isSolarDataEnabled(bool)),
            this, SLOT(isSolarDataEnabledSlot(bool)));
    connect(liveSource, SIGNAL(stationName(QString)),
            this, SLOT(stationNameSlot(QString)));
    connect(liveSource, SIGNAL(newImage(NewImageInfo)),
            this, SLOT(newImageSlot(NewImageInfo)));
    connect(liveSource, SIGNAL(newSample(Sample)),
            this, SLOT(newSampleSlot(Sample)));

    // Hook up sample signals
    if (liveSource != sampleSource) {
        // These signals exist on and are used by both
        connect(sampleSource, SIGNAL(error(QString)),
                this, SLOT(errorSlot(QString)));
    }
    connect(sampleSource, SIGNAL(samplesReady(SampleSet)),
            this, SLOT(samplesReadySlot(SampleSet)));
    connect(sampleSource, SIGNAL(rainTotalsReady(QDate,double,double,double)),
            this, SLOT(rainTotalsReadySlot(QDate,double,double,double)));
    connect(sampleSource, SIGNAL(imageDatesReady(QList<ImageDate>,QList<ImageSource>)),
            this, SLOT(imageDatesReadySlot(QList<ImageDate>,QList<ImageSource>)));
    connect(sampleSource, SIGNAL(imageListReady(QList<ImageInfo>)),
            this, SLOT(imageListReadySlot(QList<ImageInfo>)));
    connect(sampleSource, SIGNAL(imageReady(ImageInfo,QImage,QString)),
            this, SLOT(imageReadySlot(ImageInfo,QImage,QString)));
    connect(sampleSource, SIGNAL(thumbnailReady(int,QImage)),
            this, SLOT(thumbnailReadySlot(int,QImage)));
    connect(sampleSource, SIGNAL(sampleRetrievalError(QString)),
            this, SLOT(sampleRetrievalErrorSlot(QString)));
    connect(sampleSource, SIGNAL(activeImageSourcesAvailable()),
            this, SLOT(activeImageSourcesAvailableSlot()));
    connect(sampleSource, SIGNAL(archivedImagesAvailable()),
            this, SLOT(archivedImagesAvailableSlot()));
}

void DataSourceProxy::enableLiveData() {
    if (liveSource == 0) {
        return; // ERROR
    }

    liveSource->enableLiveData();
}

void DataSourceProxy::disableLiveData() {
    if (liveSource == 0) {
        return; // ERROR
    }

    liveSource->disableLiveData();
}


void DataSourceProxy::fetchSamples(
            SampleColumns columns,
            QDateTime startTime,
            QDateTime endTime,
            AggregateFunction aggregateFunction,
            AggregateGroupType groupType,
            uint32_t groupMinutes) {
    if (sampleSource == 0) {
        return; // ERROR
    }

    sampleSource->fetchSamples(columns, startTime, endTime, aggregateFunction,
                               groupType, groupMinutes);
}

hardware_type_t DataSourceProxy::getHardwareType() {
    if (sampleSource == 0) {
        return HW_GENERIC; // ERROR
    }

    return sampleSource->getHardwareType();
}

void DataSourceProxy::fetchImageDateList() {
    if (sampleSource == 0) {
        return; // ERROR
    }

    sampleSource->fetchImageDateList();
}

void DataSourceProxy::fetchImageList(QDate date, QString imageSourceCode) {
    if (sampleSource == 0) {
        return; // ERROR
    }

    sampleSource->fetchImageList(date, imageSourceCode);
}

void DataSourceProxy::fetchImage(int imageId) {
    if (sampleSource == 0) {
        return; // ERROR
    }

    sampleSource->fetchImage(imageId);
}

void DataSourceProxy::fetchThumbnails(QList<int> imageIds) {
    if (sampleSource == 0) {
        return; // ERROR
    }

    sampleSource->fetchThumbnails(imageIds);
}

void DataSourceProxy::fetchLatestImages() {
    if (sampleSource == 0) {
        return; // ERROR
    }
    sampleSource->fetchLatestImages();
}

void DataSourceProxy::hasActiveImageSources() {
    if (sampleSource == 0) {
        return; // ERROR
    }
    sampleSource->hasActiveImageSources();
}

void DataSourceProxy::fetchRainTotals() {
    if (sampleSource == 0) {
        return; // ERROR
    }
    sampleSource->fetchRainTotals();
}

void DataSourceProxy::fetchSamplesFromCache(DataSet dataSet) {
    if (sampleSource == 0) {
        return; // ERROR
    }
    sampleSource->fetchSamplesFromCache(dataSet);
}

QSqlQuery DataSourceProxy::query() {
    if (sampleSource == 0) {
        return QSqlQuery(); // ERROR
    }
    return sampleSource->query();
}

void DataSourceProxy::primeCache(QDateTime start, QDateTime end, bool imageDates) {
    if (sampleSource == 0) {
        return; // ERROR
    }
    sampleSource->primeCache(start, end, imageDates);
}

bool DataSourceProxy::solarAvailable() {
    if (sampleSource == 0) {
        return false; // ERROR
    }

    return sampleSource->solarAvailable();
}

station_info_t DataSourceProxy::getStationInfo() {
    if (sampleSource == 0) {
        station_info_t info;
        info.isValid = false;
        return info;
    }
    return sampleSource->getStationInfo();
}

// Slots
void DataSourceProxy::liveDataSlot(LiveDataSet data) {
    emit liveData(data);
}

void DataSourceProxy::errorSlot(QString errMsg) {
    emit error(errMsg);
}

void DataSourceProxy::isSolarDataEnabledSlot(bool enabled) {
    emit isSolarDataEnabled(enabled);
}

void DataSourceProxy::stationNameSlot(QString name) {
    emit stationName(name);
}

void DataSourceProxy::newImageSlot(NewImageInfo imageInfo) {
    emit newImage(imageInfo);
}

void DataSourceProxy::newSampleSlot(Sample sample) {
    emit newSample(sample);
}

void DataSourceProxy::samplesReadySlot(SampleSet samples) {
    emit samplesReady(samples);
}

void DataSourceProxy::rainTotalsReadySlot(QDate date, double day, double month, double year) {
    emit rainTotalsReady(date, day, month, year);
}

void DataSourceProxy::imageDatesReadySlot(QList<ImageDate> imageDates,
                     QList<ImageSource> imageSources) {
    emit imageDatesReady(imageDates, imageSources);
}

void DataSourceProxy::imageListReadySlot(QList<ImageInfo> images) {
    emit imageListReady(images);
}

void DataSourceProxy::imageReadySlot(ImageInfo imageInfo, QImage image, QString filename) {
    emit imageReady(imageInfo, image, filename);
}

void DataSourceProxy::thumbnailReadySlot(int imageId, QImage thumbnail) {
    emit thumbnailReady(imageId, thumbnail);
}

void DataSourceProxy::sampleRetrievalErrorSlot(QString message) {
    emit sampleRetrievalError(message);
}

void DataSourceProxy::activeImageSourcesAvailableSlot() {
    emit activeImageSourcesAvailable();
}

void DataSourceProxy::archivedImagesAvailableSlot() {
    emit archivedImagesAvailable();
}

sample_range_t DataSourceProxy::getSampleRange() {
    if (sampleSource == 0) {
        sample_range_t info;
        info.isValid = false;
        return info;
    }
    return sampleSource->getSampleRange();
}
