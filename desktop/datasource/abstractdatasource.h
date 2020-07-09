#ifndef ABSTRACTDATASOURCE_H
#define ABSTRACTDATASOURCE_H

#include <QObject>
#include <QDateTime>
#include <QWidget>
#include <QList>
#include <QImage>
#include <QPointer>
#include <QSqlQuery>

#include "abstractlivedatasource.h"
#include "sampleset.h"
#include "imageset.h"
#include "abstractprogresslistener.h"
#include "station_info.h"

class AbstractDataSource : public AbstractLiveDataSource
{
    Q_OBJECT
public:
    explicit AbstractDataSource(AbstractProgressListener* progressListener = 0, QObject *parent = 0);

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

    /** Creates a query that can used against the backing database.
     */
    virtual QSqlQuery query() = 0;

    /** Fetches samples from the cache exclusively. If the cache is missing data
     * then that data will not be returned. Call primeCache() to ensure the desired
     * data is available.
     *
     * This allows cache to be refreshed once (via primeCache()) for multiple
     * sample queries rather than having the cache refreshed for every query
     * as with fetchSamples()
     *
     * For the DatabaseDataSource this function just delegates to fetchSamples()
     * and the primeCache() function does nothing.
     *
     * @param dataSet Dataset to fetch.
     */
    virtual void fetchSamplesFromCache(DataSet dataSet) = 0;

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

    /** Obtains the total rainfall for the day, month and year. Results will
     * come from the rainTotalsReady signal.
     */
    virtual void fetchRainTotals() = 0;

    /** Ensures all samples within the specified timespan are available in the cache.
     *
     * @param start Start time
     * @param end End time
     * @param imageDates Load a list of dates where images are available
     */
    virtual void primeCache(QDateTime start, QDateTime end, bool imageDates) = 0;

    /** Returns true if solar data is available for this station.
     *
     * @return True if UV and Solar Radiation sensors are available.
     */
    virtual bool solarAvailable() = 0;

    /** Returns the set of extra columns available for the station.
     *
     * @return
     */
    virtual ExtraColumns extraColumnsAvailable() = 0;

    virtual QMap<ExtraColumn, QString> extraColumnNames() = 0;

    /** Gets basic station information
     *
     * @return Station information
     */
    virtual station_info_t getStationInfo() = 0;

    /** Gets the timespan for which samples are available. Queries for samples outside
     * this timespan will produce no results. The upper bound of this range is likely to
     * change every 5 minutes or so.
     * @return Sample timespan
     */
    virtual sample_range_t getSampleRange() = 0;
signals:
    /** Emitted when samples have been retrieved and are ready for processing.
     *
     * @param samples Requested samples.
     */
    void samplesReady(SampleSet samples);

    /** Emitted in response to the fetchRainTotals() method.
     *
     * @param date Date the rain totals are for
     * @param day Day total
     * @param month Month total
     * @param year Year total
     */
    void rainTotalsReady(QDate date, double day, double month, double year);

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
     * @param filename Name of cached copy on disk (if it exists)
     */
    void imageReady(ImageInfo imageInfo, QImage image, QString filename);

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

    /** Emitted when a primeCache() call has completed.
     *
     */
    void cachingFinished();

    /** Emitted when connecting the samples data source fails.
     *
     * @param errMsg Error message
     */
    void samplesConnectFailed(QString errMsg);

protected:
    QPointer<AbstractProgressListener> progressListener;
};



#endif // ABSTRACTDATASOURCE_H
