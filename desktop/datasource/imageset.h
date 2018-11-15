#ifndef IMAGESET_H
#define IMAGESET_H

#include <QDate>
#include <QDateTime>
#include <QStringList>

struct ImageDate {
    QDate date;
    QStringList sourceCodes;
    //QStringList mimeTypes;
};

struct ImageSource {
    QString code;
    QString name;
    QString description;
};

struct ImageInfo {
    int id;
    QDateTime timeStamp;
    QString imageTypeCode;
    QString imageTypeName;

    QString title;
    QString description;
    QString mimeType;
    ImageSource imageSource;
    QString fullUrl; /*!< Only used by the WebDataSource caching system */

    bool hasMetadata;
    QString metadata;
    QString metaUrl; /*!< Only used by the WebDataSource caching system */
};

bool imageLessThan(const ImageInfo &i1, const ImageInfo &i2);
bool imageGreaterThan(const ImageInfo &i1, const ImageInfo &i2);

#endif // IMAGESET_H

