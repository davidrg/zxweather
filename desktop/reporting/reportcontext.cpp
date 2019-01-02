#include "reportcontext.h"

#include <QtDebug>

ReportContext::ReportContext(QVariant& root,
              Mustache::PartialResolver* resolver,
              QSharedPointer<ScriptingEngine> engine):
    Mustache::QtVariantContext(root, resolver),
    engine(engine),
    reportData(root) {

}

bool ReportContext::canEval(const QString &key) const {
    ScriptValue globalObject = engine->globalObject();
    bool result = globalObject.hasProperty(key) && globalObject.property(key).isCallable();

    //qDebug() << "Lambda: canEval" << key << result;

    return result;
}

QString ReportContext::eval(const QString &key, const QString &_template, Mustache::Renderer *renderer) {
    QJSValueList args;

    args << engine->toScriptValue(_template);
    args << engine->newQObject(new ScriptRenderWrapper(renderer, this, reportData));

    ScriptValue globalObject = engine->globalObject();
    ScriptValue callResult = globalObject.property(key).call(args);

    if (callResult.isError()) {
        qWarning() << "Error running template lambda" << key << callResult.toString();
        return QString();
    }
    return callResult.toString();
}
