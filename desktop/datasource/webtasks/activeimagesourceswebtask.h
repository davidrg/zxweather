#ifndef ACTIVEIMAGESOURCESWEBTASK_H
#define ACTIVEIMAGESOURCESWEBTASK_H

#include <QObject>

#include "abstractwebtask.h"

/** This task finds finds out if a particular station has any active or
 * inactive image sources.
 */
class ActiveImageSourcesWebTask : public AbstractWebTask
{
    Q_OBJECT
public:
    /** Constucts a new ActiveImageSourcesWebTask
     *
     * @param baseUrl The base URL for the web interface
     * @param stationCode Station Code for the weather station being used
     * @param ds Parent data source that this task is doing work for
     */
    explicit ActiveImageSourcesWebTask(QString baseUrl, QString stationCode,
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
    QString supertaskName() const { return "Checking image sources"; }

    /** Name of this task. Used as the first line in a one line progress dialog
     * or the second line (shared with subtasks) in a two-line progress dialog.
     *
     * @return Name of this task or this tasks first subtask.
     */
    QString taskName() const {
        return "Downloading image source configuration data";
    }

signals:

     /** Emitted upon completion of this task to indicate there are archived
      * images available for viewing from one or more image sources.
      */
     void archivedImagesAvailable();

     /** Indicates there is one or more active image sources available
      */
     void activeImageSourcesAvailable();

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

#endif // IMAGESOURCELISTWEBTASK_H
