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

AppLock::~AppLock() {
    if (peer != NULL) {
        delete peer;
    }
}


void AppLock::relock(const QString &newLockId) {
    if (peer != NULL) {
        delete peer;
        peer = NULL;
    }

    qDebug() << "relocking with new app Id" << newLockId;

    lock(newLockId);

    if (isRunning()) {
        qWarning() << "Relock failed: another instance is already running with app id" << newLockId;
    }
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
