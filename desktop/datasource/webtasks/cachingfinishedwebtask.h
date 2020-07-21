#ifndef CACHINGFINISHEDWEBTASK_H
#define CACHINGFINISHEDWEBTASK_H

#include "abstractwebtask.h"

class CachingFinishedWebTask : public AbstractWebTask
{
public:
    CachingFinishedWebTask(QString baseUrl, QString stationCode, WebDataSource* ds);

    void beginTask();

    QString taskName() const {
        return tr("Finished Caching");
    }

public slots:
    virtual void networkReplyReceived(QNetworkReply *reply) { Q_UNUSED(reply); }
};

#endif // CACHINGFINISHEDWEBTASK_H
