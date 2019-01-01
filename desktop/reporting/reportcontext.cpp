#include "reportcontext.h"

#include <QtDebug>

#ifdef USE_QJSENGINE
ReportContext::ReportContext(QVariant& root,
              Mustache::PartialResolver* resolver,
              QJSEngine &engine):
    Mustache::QtVariantContext(root, resolver),
    engine(engine),
    reportData(root) {

}
#else
ReportContext::ReportContext(QVariant& root,
              Mustache::PartialResolver* resolver,
              QScriptEngine &engine):
    Mustache::QtVariantContext(root, resolver),
    engine(engine),
    reportData(root) {
}
#endif

bool ReportContext::canEval(const QString &key) const {
    ScriptValue globalObject = engine.globalObject();
    bool result = globalObject.hasProperty(key) && globalObject.property(key).isCallable();

    //qDebug() << "Lambda: canEval" << key << result;

    return result;
}

QString ReportContext::eval(const QString &key, const QString &_template, Mustache::Renderer *renderer) {
#ifdef USE_QJSENGINE
    QJSValueList args;
#else
    QScriptValueList args;
#endif

    args << engine.toScriptValue(_template);

#ifdef USE_QJSENGINE
    args << engine.newQObject(new ScriptRenderWrapper(renderer, this, reportData));
#else
    args << engine.newQObject(
                new ScriptRenderWrapper(renderer, this, reportData),
                QScriptEngine::AutoOwnership,
                QScriptEngine::ExcludeChildObjects |
                    QScriptEngine::ExcludeDeleteLater |
                    QScriptEngine::ExcludeSuperClassContents |
                    QScriptEngine::ExcludeSuperClassMethods |
                    QScriptEngine::ExcludeSuperClassProperties);
#endif

    ScriptValue globalObject = engine.globalObject();
    ScriptValue callResult = globalObject.property(key).call(args);

    if (callResult.isError()) {
        qWarning() << "Error running template lambda" << key << callResult.toString();
        return QString();
    }
    return callResult.toString();
}
