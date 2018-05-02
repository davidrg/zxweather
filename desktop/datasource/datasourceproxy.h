#ifndef DATASOURCEPROXY_H
#define DATASOURCEPROXY_H

#include <QObject>

#include "abstractdatasource.h"

/** Proxy class to allow two different data sources to be used for live and
 * sample data. This is required when, for example, using the TcpLiveDataSource
 * for live data and the WebDataSource for samples instead of using the
 * WebDataSource for both (as the WebDataSource only provides updates every
 * minute compared to every 2.5 seconds for the TcpLiveDataSource)
 *
 * To use, configure the data source type to use for the two types of data then
 * call connectDataSources()
 */
class DataSourceProxy : public AbstractDataSource
{
    Q_OBJECT

public:
    DataSourceProxy(AbstractProgressListener *progressListener = 0, QObject *parent = 0);

    typedef enum {
        LDST_DATABASE,
        LDST_WEB,
        LDST_TCP
    } LiveDataSourceType;

    typedef enum {
        DST_DATABASE,
        DST_WEB
    } DataSourceType;

    void setDataSourceTypes(LiveDataSourceType live, DataSourceType data);

    void connectDataSources();

    // Reimplemented members from AbstractLiveDataSource;
    void enableLiveData();

    // Reimplemented members from AbstractDataSource:
    void fetchSamples(
                SampleColumns columns,
                QDateTime startTime,
                QDateTime endTime=QDateTime::currentDateTime(),
                AggregateFunction aggregateFunction = AF_None,
                AggregateGroupType groupType = AGT_None,
                uint32_t groupMinutes = 0);

    // This one is forwarded to the sample data source rather than live source
    // as it should provide a more reliable answer
    hardware_type_t getHardwareType();

    void fetchImageDateList();
    void fetchImageList(QDate date, QString imageSourceCode);
    void fetchImage(int imageId);
    void fetchThumbnails(QList<int> imageIds);
    void fetchLatestImages();
    void hasActiveImageSources();
    void fetchRainTotals();
    void fetchSamplesFromCache(DataSet dataSet);
    QSqlQuery query();
    void primeCache(QDateTime start, QDateTime end);

private slots:
    // Forward AbstractLiveDataSource signals
    void liveDataSlot(LiveDataSet data);
    void newImageSlot(NewImageInfo imageInfo);
    void newSampleSlot(Sample sample);

    void errorSlot(QString errMsg); // also used by sample functions
    void isSolarDataEnabledSlot(bool enabled);
    void stationNameSlot(QString name);


    // Forward AbstractDataSource signals
    void samplesReadySlot(SampleSet samples);
    void rainTotalsReadySlot(QDate date, double day, double month, double year);
    void imageDatesReadySlot(QList<ImageDate> imageDates,
                         QList<ImageSource> imageSources);
    void imageListReadySlot(QList<ImageInfo> images);
    void imageReadySlot(ImageInfo imageInfo, QImage image, QString filename);
    void thumbnailReadySlot(int imageId, QImage thumbnail);
    void sampleRetrievalErrorSlot(QString message);
    void activeImageSourcesAvailableSlot();
    void archivedImagesAvailableSlot();

private:
    LiveDataSourceType liveType;
    DataSourceType sampleType;

    AbstractDataSource *sampleSource;
    AbstractLiveDataSource *liveSource;
};

#endif // DATASOURCEPROXY_H
