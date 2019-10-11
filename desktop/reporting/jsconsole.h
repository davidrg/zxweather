#ifndef JSCONSOLE_H
#define JSCONSOLE_H

#include <QObject>

class JSConsole : public QObject
{
    Q_OBJECT
public:
    explicit JSConsole(QObject *parent = NULL);

    Q_INVOKABLE void log(QString msg);
};

#endif // JSCONSOLE_H
