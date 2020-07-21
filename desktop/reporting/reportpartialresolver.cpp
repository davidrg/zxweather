#include "reportpartialresolver.h"

#include <QDir>

QString readTextFile(QString name, QString reportName); // defined in report.cpp

ReportPartialResolver::ReportPartialResolver(QString reportName) {
    this->reportName = reportName;

}

QString ReportPartialResolver::getPartial(const QString &name) {
    if (cache.contains(name)) {
        return cache[name];
    }

    QString partialsDir = QString("partials") + QDir::separator();

    cache[name] = readTextFile(partialsDir + name + ".mustache", reportName);

    return cache[name];
}
