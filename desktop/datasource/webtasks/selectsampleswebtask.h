#ifndef SELECTSAMPLESWEBTASK_H
#define SELECTSAMPLESWEBTASK_H

#include "abstractwebtask.h"
#include "request_data.h"

#include <QObject>

/** Final task in the FetchSamplesWebTask - selects the final dataset
 * from the cache database and returns it to the WebDataSource.
 *
 */
class SelectSamplesWebTask : public AbstractWebTask
{
    Q_OBJECT
public:
    /** Constucts a new AbstractWebTask
     *
     * @param baseUrl The base URL for the web interface
     * @param stationCode Station Code for the weather station being used
     * @param ds Parent data source that this task is doing work for
     */
    explicit SelectSamplesWebTask(QString baseUrl,
                                  QString stationCode,
                                  request_data_t requestData,
                                  WebDataSource* ds);

    /** Starts processing this task.
     */
    virtual void beginTask();

    /** This is the name of the supertask this task is a part of. It is used
     * as the first line in a two line progress dialog while this task is
     * running. It can be used to group a sequence of related tasks together
     * under one heading with the taskName() (and any values from
     * subtaskChanged()) providing subheadings as the task processes.
     *
     * @return Supertask name
     */
    virtual QString supertaskName() const { return tr("Downloading data sets..."); }

    /** Name of this task. Used as the first line in a one line progress dialog
     * or the second line (shared with subtasks) in a two-line progress dialog.
     *
     * @return Name of this task or this tasks first subtask.
     */
    virtual QString taskName() const { return tr("Select dataset"); }

public slots:
    /** Called when a network reply for a request this task submitted has
     * been received.
     *
     * It is the tasks responsibility to delete this reply once it has finished
     * with it.
     *
     * @param reply The network reply
     */
    virtual void networkReplyReceived(QNetworkReply *reply);

    // TODO: Some way of linking this network error with a request?
    // What happens if a task submits more than one request? this could get
    // confusing.

    /** Called when a network error for a submitted request has occurred.
     */
    virtual void networkError(QNetworkReply::NetworkError code) {
        Q_UNUSED(code)
        emit failed(tr("Network error"));
    }

private:
    request_data_t _requestData;
};

#endif // SELECTSAMPLESWEBTASK_H
