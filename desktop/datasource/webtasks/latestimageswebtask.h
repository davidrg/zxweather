#ifndef LATESTIMAGESWEBTASK_H
#define LATESTIMAGESWEBTASK_H

#include "abstractwebtask.h"

#include <QObject>

class LatestImagesWebTask : public AbstractWebTask
{
    Q_OBJECT
public:
    /** Constucts a new LatestImagesWebTask
     *
     * @param baseUrl The base URL for the web interface
     * @param stationCode Station Code for the weather station being used
     * @param ds Parent data source that this task is doing work for
     */
    explicit LatestImagesWebTask(QString baseUrl, QString stationCode,
                                       WebDataSource* ds);

    /** Starts processing this task.
     */
    void beginTask();

    /** This is the name of the supertask this task is a part of. It is used
     * as the first line in a two line progress dialog while this task is
     * running. It can be used to group a sequence of related tasks together
     * under one heading with the taskName() (and any values from
     * subtaskChanged()) providing subheadings as the task processes.
     *
     * @return Supertask name
     */
    QString supertaskName() const { return tr("Get Latest Images"); }

    /** Name of this task. Used as the first line in a one line progress dialog
     * or the second line (shared with subtasks) in a two-line progress dialog.
     *
     * @return Name of this task or this tasks first subtask.
     */
    QString taskName() const {
        return tr("Downloading image source configuration data");
    }

public slots:
    /** Called when a network reply for a request this task submitted has
     * been received.
     *
     * @param reply The network reply
     */
    void networkReplyReceived(QNetworkReply *reply);

private:
    void processResponse(QByteArray data);
};

#endif // LATESTIMAGESWEBTASK_H
