#include "jsconsole.h"

#include <QtDebug>

JSConsole::JSConsole(QObject *parent) : QObject(parent)
{

}

void JSConsole::log(QString msg) {
    qDebug() << msg;
}
