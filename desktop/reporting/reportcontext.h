#ifndef REPORTCONTEXT_H
#define REPORTCONTEXT_H

#include "reporting/qt-mustache/mustache.h"
#include "scriptvalue.h"
#include "scriptrenderwrapper.h"

class ReportContext : public Mustache::QtVariantContext {
public:
#ifdef USE_QJSENGINE
    ReportContext(QVariant& root,
                  Mustache::PartialResolver* resolver,
                  QJSEngine &engine);
#else
    ReportContext(QVariant& root,
                  Mustache::PartialResolver* resolver,
                  QScriptEngine &engine);
#endif

    virtual bool canEval(const QString &key) const;

    virtual QString eval(const QString &key, const QString &_template, Mustache::Renderer *renderer);

private:
#if USE_QJSENGINE
    QJSEngine &engine;
#else
    QScriptEngine &engine;
#endif

    QVariant &reportData;
};

#endif // REPORTCONTEXT_H
