#ifndef REPORTPARTIALRESOLVER_H
#define REPORTPARTIALRESOLVER_H

#include "reporting/qt-mustache/mustache.h"

class ReportPartialResolver : public Mustache::PartialResolver
{
public:
    ReportPartialResolver(QString reportName);
    virtual ~ReportPartialResolver() {}

    /** Returns the partial template with a given @p name. */
    virtual QString getPartial(const QString& name);
private:
    QString reportName;
    QHash<QString, QString> cache;
};
#endif // REPORTPARTIALRESOLVER_H
