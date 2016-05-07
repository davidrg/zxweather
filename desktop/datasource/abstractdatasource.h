#ifndef ABSTRACTDATASOURCE_H
#define ABSTRACTDATASOURCE_H

#include <QObject>
#include <QDateTime>
#include <QProgressDialog>
#include <QWidget>
#include <QList>
#include <QImage>

#include "abstractlivedatasource.h"
#include "sampleset.h"

#define THUMBNAIL_WIDTH 304
#define THUMBNAIL_HEIGHT 171

struct ImageDate {
    QDate date;
    QStringList sourceCodes;
    QStringList mimeTypes;
};

struct ImageSource {
    QString code;
    QString name;
    QString description;
};

struct ImageInfo {
    int id;
    QDateTime timeStamp;
    QString imageTypeCode;
    QString title;
    QString description;
    QString mimeType;
};

class AbstractDataSource : public AbstractLiveDataSource
{
    Q_OBJECT
public:
    explicit AbstractDataSource(QWidget* parentWidget = 0, QObject *parent = 0);

    /** Gets all samples for the supplied time range. When the samples have
     * finished downloading the samplesReady signal will be emitted with the
     * requested sample set.
     *
     * @param columns The columns to include in the dataset.
     * @param startTime Minimum timestamp.
     * @param endTime Maximum timestamp. Defaults to now.
     * @return A set of all samples between the two timestamps (inclusive)
     */
    virtual void fetchSamples(
            SampleColumns columns,
            QDateTime startTime,
            QDateTime endTime=QDateTime::currentDateTime(),
            AggregateFunction aggregateFunction = AF_None,
            AggregateGroupType groupType = AGT_None,
            uint32_t groupMinutes = 0) = 0;

    /* Convenience function - gets the sample set described by the supplied
     * DataSet
     * @param dataSet DataSet describing the samples to fetch
     */
    virtual void fetchSamples(DataSet dataSet);

    virtual hardware_type_t getHardwareType() = 0;

    virtual void fetchImageDateList() = 0;

    virtual void fetchImageList(QDate date, QString imageSourceCode) = 0;

    virtual void fetchImage(int imageId) = 0;

    virtual void fetchThumbnails(QList<int> imageIds) = 0;
signals:
    /** Emitted when samples have been retrieved and are ready for processing.
     *
     * @param samples Requested samples.
     */
    void samplesReady(SampleSet samples);

    /** Emitted with the results of fetchImageDateList().
     */
    void imageDatesReady(QList<ImageDate> imageDates,
                         QList<ImageSource> imageSources);

    void imageListReady(QList<ImageInfo> images);

    void imageReady(int imageId, QImage image);

    void thumbnailReady(int imageId, QImage thumbnail);

    /** Emitted when an error occurs during sample retrieval which forced the
     * request to be aborted.
     *
     * @param message Message describing the error.
     */
    void sampleRetrievalError(QString message);
protected:
    QScopedPointer<QProgressDialog> progressDialog;
};



#endif // ABSTRACTDATASOURCE_H
