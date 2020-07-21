#ifndef FETCHDAYIMAGESWEBTASK_H
#define FETCHDAYIMAGESWEBTASK_H

#include "abstractwebtask.h"
#include "datasource/imageset.h"

#include <QObject>
#include <QList>
#include <QDate>
#include <QString>

class ListDayImagesWebTask : public AbstractWebTask
{
    Q_OBJECT
public:
    explicit ListDayImagesWebTask(QString baseUrl, QString stationCode,
                                  WebDataSource* ds, QDate date,
                                  QString imageSourceCode);

    void beginTask();

    QString supertaskName() const { return tr("Get Image List"); }

    int subtasks() const { return 5; }

    QString taskName() const {
        return tr("Downloading image source configuration");
    }


signals:
    void imageListReady(QList<ImageInfo> images);

public slots:
    void networkReplyReceived(QNetworkReply *reply);

private:
    QString _url;
    QString _imagesRoot;
    QDate _date;
    QString _imageSourceCode;
    bool _gotImageSourceInfo;
    bool _gotCacheStatus;

    ImageSource imageSource;

    void imageSourceInfoRequestFinished(QNetworkReply *reply);
    void cacheStatusRequestFinished(QNetworkReply *reply);
    void downloadRequestFinished(QNetworkReply *reply);

    void getDataset();
    void returnImageListAndFinish();
};

#endif // FETCHDAYIMAGESWEBTASK_H
