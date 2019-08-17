#ifdef SINGLE_INSTANCE

#include "applock.h"
#include <QWidget>
#include <qtlocalpeer.h>
#include <QtDebug>

AppLock::AppLock(QObject *parent) : QObject(parent)
{
    window = NULL;
    peer = NULL;
}

void AppLock::lock(const QString &lockId) {

    peer = new QtLocalPeer(this, lockId);
    connect(peer, SIGNAL(messageReceived(const QString&)),
            SIGNAL(messageReceived(const QString&)));
}


bool AppLock::isRunning() {
    if (peer == NULL) {
        // The lock hasn't been taken yet so safest to just assume an
        // instance is already running.
        return true;
    }
    return peer->isClient();
}

void AppLock::setWindow(QWidget* window) {
    if (peer == NULL) {
        qCritical() << "Not locked - unable to set window";
        return;
    }
    this->window = window;
    connect(peer, SIGNAL(messageReceived(const QString&)),
        this, SLOT(activateWindow()));
}


bool AppLock::sendMessage(const QString &message, int timeout) {
   if (peer == NULL) {
        qCritical() << "Not locked - unable to send message";
        return false;
    }
    return peer->sendMessage(message, timeout);
}

void AppLock::activateWindow() {
    if (window != NULL) {
        window->setWindowState(window->windowState() & ~Qt::WindowMinimized);
        window->raise();
        window->activateWindow();
    }
}

#endif
