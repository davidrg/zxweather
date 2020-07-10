#ifndef FETCHSAMPLESSUBTASKS_H
#define FETCHSAMPLESSUBTASKS_H

#include "abstractwebtask.h"
#include "request_data.h"

#include <QObject>

class DataFileWebTask : public AbstractWebTask
{
    Q_OBJECT
public:
    /** Constucts a new AbstractWebTask
     *
     * @param baseUrl The base URL for the web interface
     * @param stationCode Station Code for the weather station being used
     * @param ds Parent data source that this task is doing work for
     * @param sampleInterval Stations sample interval in seconds
     */
    explicit DataFileWebTask(QString baseUrl,
                             QString stationCode,
                             request_data_t requestData,
                             QString name,
                             QString url,
                             bool forceDownload,
                             int sampleInterval,
                             WebDataSource* ds);

    /** Starts processing this task.
     */
    void beginTask();

    /** This is the maximum number of subtasks this task could perform. It will
     * be called shortly before beginTask(). It is used to calculate how much
     * space on a progress bar this task should be assigned.
     *
     * @return The maximum number of subtasks this task could perform.
     */
    int subtasks() const { return 3; }

    /** Name of this task. Used as the first line in a one line progress dialog
     * or the second line (shared with subtasks) in a two-line progress dialog.
     *
     * @return Name of this task or this tasks first subtask.
     */
    QString taskName() const {
        return "Checking cache status of " + _name;
    }

    /** Compares the result from an HTTP HEAD request to the cache database
     * and determinse if the local cache for the resource is out of date
     *
     * @param reply HTTP HEAD response
     * @return True if the cache is out-of-date and the URL needs to be downloaded
     */
    static bool UrlNeedsDownlodaing(QNetworkReply *reply);

public slots:
    /** Called when a network reply for a request this task submitted has
     * been received.
     *
     * It is the tasks responsibility to delete this reply once it has finished
     * with it.
     *
     * @param reply The network reply
     */
    void networkReplyReceived(QNetworkReply *reply);

private:
    // Parameters
    request_data_t _requestData;
    QString _url;
    QString _name;

    bool _downloadingDataset;
    bool _forceDownload;
    int _sampleInterval;

    void cacheStatusRequestFinished(QNetworkReply *reply);
    void downloadRequestFinished(QNetworkReply *reply);
    void getDataset();
    data_file_t loadDataFile(QStringList fileData,
                             QDateTime lastModified, int fileSize,
                             cache_stats_t cacheStats);
};

#endif // FETCHSAMPLESSUBTASKS_H

