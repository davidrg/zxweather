#ifndef ABSTRACTPROGRESSLISTENER_H
#define ABSTRACTPROGRESSLISTENER_H

#include <QObject>

class AbstractProgressListener : public QObject
{
    Q_OBJECT
public:
    explicit AbstractProgressListener(QObject *parent = NULL);

    virtual int value() const = 0;
    virtual int maximum() const = 0;
    virtual bool wasCanceled() const = 0;


public slots:
    virtual void setWindowTitle(QString title) = 0;
    virtual void setLabelText(QString label) = 0;

    virtual void setMaximum(int max) = 0;
    virtual void setRange(int min, int max) = 0;
    virtual void setValue(int value) = 0;

    virtual void reset() = 0;

    virtual void show() = 0;
    virtual void hide() = 0;
    virtual void close() = 0;
};

#endif // ABSTRACTPROGRESSLISTENER_H
