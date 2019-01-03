#ifndef RANGEREQUESTWEBTASK_H
#define RANGEREQUESTWEBTASK_H

#include "abstractwebtask.h"
#include "request_data.h"

#include <QObject>
#include <QSet>

// Check cache status for all URLS in parallel as part of the range request job
// rather than leaving it to individual DataFileWebTasks. This is a fair bit faster
// when latency is an issue but doesn't report progress very well at the moment.
#define PARALLEL_HEAD

class RangeRequestWebTask : public AbstractWebTask
{
    Q_OBJECT
public:
    /** Constucts a new AbstractWebTask
     *
     * @param baseUrl The base URL for the web interface
     * @param stationCode Station Code for the weather station being used
     * @param select If the data should be selected out at the end or if we're just priming the cache DB.
     * @param ds Parent data source that this task is doing work for
     */
    explicit RangeRequestWebTask(QString baseUrl,
                                 QString stationCode,
                                 request_data_t requestData,
                                 bool select,
                                 WebDataSource* ds);

    /** Starts processing this task.
     */
    void beginTask();

    /** Name of this task. Used as the first line in a one line progress dialog
     * or the second line (shared with subtasks) in a two-line progress dialog.
     *
     * @return Name of this task or this tasks first subtask.
     */
    QString taskName() const {
        return "Checking data range";
    }

    int subtasks() const { return 2; }

    static void ClearURLCache();

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
    bool _select;
    static QMap<QString, QDateTime> lastQuery;
    bool _requestingRange;
    QHash<QString, QString> _urlNames;

#ifdef PARALLEL_HEAD
    QSet<QString> _awaitingUrls;
    void headUrls();
    bool processHeadResponse(QNetworkReply *reply);
#endif

    bool processRangeResponse(QString data);

    static void getURLList(QString baseURL, QDateTime startTime, QDateTime endTime,
                    QStringList& urlList, QStringList& nameList);

    void queueDownloadTasks(bool forceDownload);
};

#endif // RANGEREQUESTWEBTASK_H

