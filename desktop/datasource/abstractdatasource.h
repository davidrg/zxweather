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
    //QStringList mimeTypes;
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
    ImageSource imageSource;
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

    /** Convenience function - gets the sample set described by the supplied
     * DataSet
     * @param dataSet DataSet describing the samples to fetch
     */
    virtual void fetchSamples(DataSet dataSet);

    /** Gets the hardware type for the configured station */
    virtual hardware_type_t getHardwareType() = 0;

    /** Fetches a list of all dates that have images available for
     * image sources associated with this station
     */
    virtual void fetchImageDateList() = 0;

    /** Fetches a list of all images available on a particular date
     * for a particular image source
     *
     * @param date Date to fetch an image list for
     * @param imageSourceCode Image source to fetch an image list for
     */
    virtual void fetchImageList(QDate date, QString imageSourceCode) = 0;

    /** Fetches a specific image
     *
     * @param imageId The image to fetch
     */
    virtual void fetchImage(int imageId) = 0;

    /** Fetches thumbnails for the list of images
     *
     * @param imageIds The images to fetch thumbnails for
     */
    virtual void fetchThumbnails(QList<int> imageIds) = 0;

    /** Fetches the latest image for each image source associated with
     * this station. Only images taken in the last 24 hours will be included
     * so that no longer active image sources get excluded.
     */
    virtual void fetchLatestImages() = 0;

    /** Checks to see if there are any image sources that have produced
     * images within the last 24 hours.
     *
     * If there are any image sources that have taken an image in the last
     * 24 hours activeImageSourcesAvailable() will be emitted.
     *
     * If there are archived images available, archivedImagesAvailable()
     * will be emitted.
     */
    virtual void hasActiveImageSources() = 0;
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

    /** Raised in response to fetchImageList(). Contains details for all
     * images from a particular image source on a particular date.
     *
     * @param images Image metadata records
     */
    void imageListReady(QList<ImageInfo> images);

    /** Raised in response to a call to fetchImage, fetchThumbnails or
     * fetchLatestImages.
     *
     * @param imageInfo Metadata for the image
     * @param image The full size image
     */
    void imageReady(ImageInfo imageInfo, QImage image);

    /** Raised in response to a call to fetchThumbnails.
     *
     * @param imageId ID for the image that was thumbnailed
     * @param thumbnail A thumbnail for the image
     */
    void thumbnailReady(int imageId, QImage thumbnail);

    /** Emitted when an error occurs during sample retrieval which forced the
     * request to be aborted.
     *
     * @param message Message describing the error.
     */
    void sampleRetrievalError(QString message);

    /** Raised if there are image sources associated with the
     * configured station that have taken images in the last
     * 24 hours. Raised in response to a call to hasActiveImageSources()
     *
     */
    void activeImageSourcesAvailable();

    /** Raised if there are archived images associated with this station.
     * Raised in response to a call to hasActiveImageSources()
     */
    void archivedImagesAvailable();
protected:
    QScopedPointer<QProgressDialog> progressDialog;
};



#endif // ABSTRACTDATASOURCE_H
