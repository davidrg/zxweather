#ifndef RANGEREQUESTWEBTASK_H
#define RANGEREQUESTWEBTASK_H

#include "abstractwebtask.h"
#include "request_data.h"

#include <QObject>

class RangeRequestWebTask : public AbstractWebTask
{
    Q_OBJECT
public:
    /** Constucts a new AbstractWebTask
     *
     * @param baseUrl The base URL for the web interface
     * @param stationCode Station Code for the weather station being used
     * @param ds Parent data source that this task is doing work for
     */
    explicit RangeRequestWebTask(QString baseUrl,
                                 QString stationCode,
                                 request_data_t requestData,
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

    bool processResponse(QString data);
};

#endif // RANGEREQUESTWEBTASK_H

