#ifndef ABSTRACTURLHANDLER_H
#define ABSTRACTURLHANDLER_H

#include <QUrl>

class AbstractUrlHandler {
public:
    virtual void handleUrl(QUrl url, bool solarDataAvailable, bool isWireless) = 0;
};


class NullUrlHandler : public AbstractUrlHandler {
public:
    NullUrlHandler() {}
    virtual ~NullUrlHandler() {}
    virtual void handleUrl(QUrl url, bool solarDataAvailable, bool isWireless) {
        Q_UNUSED(url);
        Q_UNUSED(solarDataAvailable);
        Q_UNUSED(isWireless);
    }
};

#endif // ABSTRACTURLHANDLER_H
