#ifndef REPORTCONTEXT_H
#define REPORTCONTEXT_H

#include "reporting/qt-mustache/mustache.h"
#include "scriptvalue.h"
#include "scriptrenderwrapper.h"
#include "scriptingengine.h"

class ReportContext : public Mustache::QtVariantContext {
public:
    ReportContext(QVariant& root,
                  Mustache::PartialResolver* resolver,
                  ScriptingEngine &engine);

    virtual bool canEval(const QString &key) const;

    virtual QString eval(const QString &key, const QString &_template, Mustache::Renderer *renderer);

private:
    ScriptingEngine &engine;
    QVariant &reportData;
};

#endif // REPORTCONTEXT_H
