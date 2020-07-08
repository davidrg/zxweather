#ifndef APPLOCK_H
#define APPLOCK_H

#ifdef SINGLE_INSTANCE
#include <QObject>

class QWidget;
class QtLocalPeer;


/** Implements single-instance locking in the same way as QtSingleApplication
 * but without having to take out the lock at the time the QApplication is
 * instantiated.
 *
 * zxweather does this primarily because the instance ID isn't known until
 * after we've created the QApplication and parsed command line arguments.
 *
 */
class AppLock : public QObject
{
    Q_OBJECT
public:
    explicit AppLock(QObject *parent = NULL);
    ~AppLock();

    void lock(const QString &lockId);

    bool isRunning();

    void setWindow(QWidget* aw);

public slots:
    bool sendMessage(const QString &message, int timeout = 5000);

    void relock(const QString &newLockId);

signals:
    void messageReceived(const QString &message);

protected slots:
    void activateWindow();

protected:
    QtLocalPeer* peer;
    QWidget *window;
};

#endif

#endif // APPLOCK_H
