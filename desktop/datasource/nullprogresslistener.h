#ifndef NULLPROGRESSLISTENER_H
#define NULLPROGRESSLISTENER_H

#include <QObject>

#include "abstractprogresslistener.h"

class NullProgressListener : public AbstractProgressListener
{
    Q_OBJECT
public:
    explicit NullProgressListener(QObject *parent = NULL);

    virtual int value() const {return this->val;}
    virtual int maximum() const {return this->max; }
    virtual bool wasCanceled() const {return false;}


public slots:
    virtual void setTaskName(QString title) {}
    virtual void setSubtaskName(QString label) {}

    virtual void setMaximum(int max) {this->max = max;}
    virtual void setRange(int min, int max) {this->max = max;}
    virtual void setValue(int value) {this->val = value;}

    virtual void reset() {}

    virtual void show() {}
    virtual void hide() {}
    virtual void close() {}

private:
    int max;
    int val;
};

#endif // NULLPROGRESSLISTENER_H
