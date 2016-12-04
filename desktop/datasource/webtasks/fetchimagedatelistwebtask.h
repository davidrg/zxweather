#ifndef FETCHIMAGEDATELISTWEBTASK_H
#define FETCHIMAGEDATELISTWEBTASK_H

#include "abstractwebtask.h"

#include <QObject>

class FetchImageDateListWebTask : public AbstractWebTask
{
    Q_OBJECT
public:
    explicit FetchImageDateListWebTask(QString baseUrl, QString stationCode,
                                       WebDataSource* ds);
    void beginTask();

    int subtasks() { return 3; }

    QString taskName() const {
        return "Loading image source information";
    }

public slots:
    void networkReplyReceived(QNetworkReply *reply);

signals:
    void dateListReady(QList<ImageDate> imageDates,
                       QList<ImageSource> imageSources);

private:
    void processStationList(QString data);
    void processDateList(QString data);
    bool _haveStationInfo;

    QList<ImageSource> _imageSources;
};

#endif // FETCHIMAGEDATELISTWEBTASK_H