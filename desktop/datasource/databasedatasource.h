#ifndef DATABASEDATASOURCE_H
#define DATABASEDATASOURCE_H

#include <QObject>
#include <QTimer>
#include <QScopedPointer>
#include "abstractdatasource.h"
#include <QSqlError>

#ifndef NO_ECPG
#include "dbsignaladapter.h"
#include "database.h"
#endif

class DatabaseDataSource : public AbstractDataSource
{
    Q_OBJECT
public:
    explicit DatabaseDataSource(AbstractProgressListener* progressListener = 0, QObject *parent = 0);
    
    ~DatabaseDataSource();

    virtual void fetchSamples(
            SampleColumns columns,
            QDateTime startTime,
            QDateTime endTime=QDateTime::currentDateTime(),
            AggregateFunction aggregateFunction = AF_None,
            AggregateGroupType groupType = AGT_None,
            uint32_t groupMinutes = 0);

    void fetchSamplesFromCache(DataSet dataSet);

    QSqlQuery query();

    void enableLiveData();
    void disableLiveData();

    hardware_type_t getHardwareType();

    void fetchImageDateList();

    void fetchImageList(QDate date, QString imageSourceCode);

    void fetchImage(int imageId);

    void fetchThumbnails(QList<int> imageIds);

    void fetchLatestImages();

    void hasActiveImageSources();

    void fetchRainTotals();

    void primeCache(QDateTime start, QDateTime end);

    bool solarAvailable();

    station_info_t getStationInfo();

    sample_range_t getSampleRange();
private slots:
#ifndef NO_ECPG
    void processLiveData(live_data_record record);
#endif
    void dbError(QString message);

    void processNewImage(int imageId);
    void processNewSample(int sampleId);
private:
    int getStationId();
    QString getStationHwType();
    void fetchImages(QList<int> imageIds, bool thumbnail);

    double nullableVariantDouble(QVariant v);
    int getSampleInterval();

    int basicCountQuery(int stationId, QDateTime startTime, QDateTime endTime);
    int groupedCountQuery(int stationId, QDateTime startTime, QDateTime endTime,
                          AggregateFunction function,
                          AggregateGroupType groupType, uint32_t minutes);

    void databaseError(QString source, QSqlError error, QString sql);

    QList<ImageDate> getImageDates(int stationId, int progressOffset);
    QList<ImageSource> getImageSources(int stationId, int progressOffset);

    //QString buildSelectForColumns(SampleColumns columns);

    int sampleInterval;

    bool liveDataEnabled;

    // Live data functionality
#ifndef NO_ECPG
    void connectToDB();
#endif
};

#endif // DATABASEDATASOURCE_H
