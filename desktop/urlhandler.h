#ifndef URLHANDLER_H
#define URLHANDLER_H

#include "abstracturlhandler.h"

class UrlHandler: public AbstractUrlHandler
{
public:
    UrlHandler();
    virtual ~UrlHandler();

    virtual void handleUrl(QUrl url, bool solarDataAvailable, bool isWireless);
};

#endif // URLHANDLER_H
