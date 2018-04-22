#ifndef ABSTRACTWEBTASK_H
#define ABSTRACTWEBTASK_H

#include <QObject>
#include "../webdatasource.h"

class QNetworkReply;
class QNetworkRequest;

/** A Task the WebDataSource needs to perform that involves talking over
 * HTTP
 *
 */
class AbstractWebTask : public QObject
{
    Q_OBJECT
public:
    /** Constucts a new AbstractWebTask
     *
     * @param baseUrl The base URL for the web interface
     * @param stationCode Station Code for the weather station being used
     * @param ds Parent data source that this task is doing work for
     */
    explicit AbstractWebTask(QString baseUrl, QString stationCode,
                             WebDataSource* ds);

    /** Starts processing this task.
     */
    virtual void beginTask() = 0;

    /** This is the maximum number of subtasks this task could perform. It will
     * be called shortly before beginTask(). It is used to calculate how much
     * space on a progress bar this task should be assigned.
     *
     * @return The maximum number of subtasks this task could perform.
     */
    virtual int subtasks() const { return 0; }

    /** This is the name of the supertask this task is a part of. It is used
     * as the first line in a two line progress dialog while this task is
     * running. It can be used to group a sequence of related tasks together
     * under one heading with the taskName() (and any values from
     * subtaskChanged()) providing subheadings as the task processes.
     *
     * @return Supertask name
     */
    virtual QString supertaskName() const { return QString(); }

    /** Name of this task. Used as the first line in a one line progress dialog
     * or the second line (shared with subtasks) in a two-line progress dialog.
     *
     * @return Name of this task or this tasks first subtask.
     */
    virtual QString taskName() const = 0;

signals:
    /** Emitted when the task has moved onto another subtask. This will progress
     * the progress bar by 1. This should only ever be emitted as many times
     * as subtasks() property indicates.
     *
     * @param name
     */
    void subtaskChanged(QString name);

    /** Emitted when this task wishes to perform an HTTP GET request. The
     * reply will be passed in via networkReplyReceived(). Any errors will be
     * passed in through networkError().
     *
     * @param request The network request to be processed.
     */
    void httpGet(QNetworkRequest request);

    /** Emitted when this task wishes to perform an HTTP HEAD request. The
     * reply will be passed in via networkReplyReceived(). Any errors will be
     * passed in through networkError().
     *
     * @param request The network request to be processed.
     */
    void httpHead(QNetworkRequest request);

    /** Queue a new WebTask for processing after this task (and any other tasks
     * already on the queue)
     *
     * @param task New task to queue.
     */
    void queueTask(AbstractWebTask* task);

    /** Emitted when this task has finished. The task will be destroyed shortly
     * after this signal is emitted (via deleteLater()) so the task should emit
     * this as the very last thing it does - it should have finished all
     * processing work before emitting it.
     */
    void finished();

    void finishedWithNewTask(AbstractWebTask* task);

    /** Emitted when this task has failed. This task will be deleted shortly
     * after this signal is emitted via deleteLater() so it should have finished
     * all processing before emitting it.
     *
     * Additionally, all tasks this task has queued will be removed from the
     * queue along with all other related tasks. The overall operation (eg,
     * fetching samples for a graph) will be aborted.
     *
     * @param errorMessage Any error message assocated with the failure
     */
    void failed(QString errorMessage);

public slots:
    /** Called when a network reply for a request this task submitted has
     * been received.
     *
     * It is the tasks responsibility to delete this reply once it has finished
     * with it.
     *
     * @param reply The network reply
     */
    virtual void networkReplyReceived(QNetworkReply *reply) = 0;

    // TODO: Some way of linking this network error with a request?
    // What happens if a task submits more than one request? this could get
    // confusing.

    /** Called when a network error for a submitted request has occurred.
     */
    virtual void networkError(QNetworkReply::NetworkError code) {
        Q_UNUSED(code)
        emit failed("Network error");
    }

    /** Cancels whatever this task is doing
     *
     */
    virtual void cancel() { }

protected:
    QString _baseUrl; /*!< Base URL for zxweather web UI */
    QString _dataRootUrl; /*!< Data section (/data) */
    QString _stationCode; /*!< Station code we're working with */
    QString _stationBaseUrl; /*!< Station data section (/data/station_code) */
    QString _stationDataUrl; /*!< Root for sample datasets (varies) */
    WebDataSource* _dataSource;
};

#endif // WEBTASK_H
