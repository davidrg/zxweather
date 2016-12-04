#ifndef FETCHTHUMBNAILWEBTASK_H
#define FETCHTHUMBNAILWEBTASK_H

#include "fetchimagewebtask.h"

#include <QObject>

class FetchThumbnailWebTask : public FetchImageWebTask
{
    Q_OBJECT
public:
    /** Constucts a new FetchThumbnailWebTask to fetch a single thumbnail of an
     * image
     *
     * @param baseUrl The base URL for the web interface
     * @param stationCode Station Code for the weather station being used
     * @param ds Parent data source that this task is doing work for
     * @param imageInfo Information for the image to fetch from either cache or
     *          the internet.
     */
    explicit FetchThumbnailWebTask(QString baseUrl, QString stationCode,
                               WebDataSource* ds, ImageInfo imageInfo);

    /** Constucts a new FetchThumbnailWebTask to fetch a single thumbnail of an
     * image
     *
     * @param baseUrl The base URL for the web interface
     * @param stationCode Station Code for the weather station being used
     * @param ds Parent data source that this task is doing work for
     * @param imageId The ID of the image to fetch. The image must already
     *      exist in the cache database.
     */
    explicit FetchThumbnailWebTask(QString baseUrl, QString stationCode,
                               WebDataSource* ds, int imageId);

protected:
    virtual void dealWithImage(QString filename);
};

#endif // FETCHTHUMBNAILWEBTASK_H
