#include "reportpartialresolver.h"

#include <QDir>

QString readTextFile(QString name); // defined in report.cpp

ReportPartialResolver::ReportPartialResolver(QString reportName) {
    partialsDir = reportName + QDir::separator() + "partials" + QDir::separator();
}

QString ReportPartialResolver::getPartial(const QString &name) {

    if (cache.contains(name)) {
        return cache[name];
    }

    cache[name] = readTextFile(partialsDir + name + ".mustache");

    return cache[name];
}
