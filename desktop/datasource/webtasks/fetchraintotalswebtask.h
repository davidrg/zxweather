#ifndef FETCHRAINTOTALSWEBTASK_H
#define FETCHRAINTOTALSWEBTASK_H

#include "abstractwebtask.h"

#include <QObject>

class FetchRainTotalsWebTask : public AbstractWebTask
{
    Q_OBJECT

public:
    FetchRainTotalsWebTask(QString baseUrl,
                           QString stationCode,
                           WebDataSource* ds);

    void beginTask();

    QString supertaskName() const {
        return tr("Downloading Rain Summary...");
    }

    QString taskName() const {
        return "";
    }

public slots:
    void networkReplyReceived(QNetworkReply *reply);
};

#endif // FETCHRAINTOTALSWEBTASK_H
