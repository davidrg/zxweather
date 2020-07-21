#ifndef FETCHIMAGEWEBTASK_H
#define FETCHIMAGEWEBTASK_H

#include "abstractwebtask.h"

#include <QObject>

class FetchImageWebTask : public AbstractWebTask
{
    Q_OBJECT
public:
    /** Constucts a new FetchImageWebTask to fetch a single image
     *
     * @param baseUrl The base URL for the web interface
     * @param stationCode Station Code for the weather station being used
     * @param ds Parent data source that this task is doing work for
     * @param imageInfo Information for the image to fetch from either cache or
     *          the internet.
     */
    explicit FetchImageWebTask(QString baseUrl, QString stationCode,
                               WebDataSource* ds, ImageInfo imageInfo);

    /** Constucts a new FetchImageWebTask to fetch a single image
     *
     * @param baseUrl The base URL for the web interface
     * @param stationCode Station Code for the weather station being used
     * @param ds Parent data source that this task is doing work for
     * @param imageId The ID of the image to fetch. The image must already
     *      exist in the cache database.
     */
    explicit FetchImageWebTask(QString baseUrl, QString stationCode,
                               WebDataSource* ds, int imageId);

    /** Starts processing this task.
     */
    void beginTask();

    /** Name of this task. Used as the first line in a one line progress dialog
     * or the second line (shared with subtasks) in a two-line progress dialog.
     *
     * @return Name of this task or this tasks first subtask.
     */
    QString taskName() const {
        QString t = "image";
        if (_imageInfo.mimeType.startsWith("video/"))
            t = "video";
        else if (_imageInfo.mimeType.startsWith("audio/"))
            t = "recording";

        return QString(tr("Downloading %1 %2"))
                .arg(t)
                .arg(_imageInfo.timeStamp.toString(Qt::SystemLocaleShortDate));
    }

public slots:
    /** Called when a network reply for a request this task submitted has
     * been received.
     *
     * @param reply The network reply
     */
    void networkReplyReceived(QNetworkReply *reply);

protected:
    QString getCacheFilename(bool thumbnail);
    virtual void dealWithImage(QString filename);

    bool needImage;

    ImageInfo _imageInfo;
    QString filename;
    bool needMetadata;
};

#endif // FETCHIMAGEWEBTASK_H
