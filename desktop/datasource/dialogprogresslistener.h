#ifndef DIALOGPROGRESSLISTENER_H
#define DIALOGPROGRESSLISTENER_H

#include <QObject>
#include <QScopedPointer>
#include <QProgressDialog>

#include "abstractprogresslistener.h"

class DialogProgressListener : public AbstractProgressListener
{
    Q_OBJECT
public:
    explicit DialogProgressListener(QWidget *parent = NULL);

    int value() const;
    int maximum() const;
    bool wasCanceled() const;


public slots:
    void setLabelText(QString label);
    void setMaximum(int max);
    void setValue(int value);
    void setWindowTitle(QString title);
    void setRange(int min, int max);

    void reset();

    void show();
    void hide();
    void close();

private:
    QScopedPointer<QProgressDialog> dialog;
};

#endif // DIALOGPROGRESSLISTENER_H
