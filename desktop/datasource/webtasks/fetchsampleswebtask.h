#ifndef FETCHSAMPLESWEBTASK_H
#define FETCHSAMPLESWEBTASK_H

#include "abstractwebtask.h"
#include "datasource/webdatasource.h"

#include <QObject>
#include <QDateTime>
#include <QByteArray>

/** Fetches a set of samples
 */
class FetchSamplesWebTask : public AbstractWebTask
{
    Q_OBJECT
public:
    /** Constucts a new AbstractWebTask
     *
     * @param baseUrl The base URL for the web interface
     * @param stationCode Station Code for the weather station being used
     * @param ds Parent data source that this task is doing work for
     */
    explicit FetchSamplesWebTask(QString baseUrl,
                                 QString stationCode,
                                 SampleColumns columns,
                                 QDateTime startTime,
                                 QDateTime endTime,
                                 AggregateFunction aggregateFunction,
                                 AggregateGroupType groupType,
                                 uint32_t groupMinutes, bool select,
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
    QString supertaskName() const {
        return "Downloading data sets...";
    }

    /** Name of this task. Used as the first line in a one line progress dialog
     * or the second line (shared with subtasks) in a two-line progress dialog.
     *
     * @return Name of this task or this tasks first subtask.
     */
    QString taskName() const {
        return "Loading system configuration";
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
    SampleColumns _columns;
    QDateTime _startTime;
    QDateTime _endTime;
    AggregateFunction _aggregateFunction;
    AggregateGroupType _groupType;
    uint32_t _groupMinutes;
    bool _select;

    // Sysconfig data
    QString _stationName;
    bool _isSolarAvailable;
    bool _isWireless;
    hardware_type_t _hwType;

    // Cache
    static QString lastSysConfig;

    bool processResponse(QByteArray responseData);

    void finishWork();
};

#endif // FETCHSAMPLESWEBTASK_H
