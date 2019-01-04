#include "scriptingengine.h"

#include <QtDebug>

QString readTextFile(QString name); // defined in report.cpp

ScriptingEngine::ScriptingEngine(QStringList scriptFiles, QObject *parent) : QObject(parent)
{
    _scripts = scriptFiles;

    initialiseScriptEngine();
}


void ScriptingEngine::initialiseScriptEngine() {
    // Load all the script files
    QString script;
    foreach (QString file, _scripts) {
        script += readTextFile(file) + "\n";
    }

    // Perform any basic initialisation
#if USE_QJSENGINE
    engine.installExtensions(QJSEngine::AllExtensions);
#else
#endif

    qDebug() << "Evaluating script...";
    ScriptValue evalResult = engine.evaluate(script, "script");

    if (evalResult.isError()) {
        qWarning() << evalResult.toString();
        return;
    }
    qDebug() << "done.";
}

ScriptValue ScriptingEngine::globalObject() {
    return engine.globalObject();
}


QJSValue ScriptingEngine::newArray(uint length) {
    return engine.newArray(length);
}

QJSValue ScriptingEngine::newObject() {
    return engine.newObject();
}

QJSValue ScriptingEngine::newQObject(QObject* obj) {
#if USE_QJSENGINE
    return engine.newQObject(obj);
#else
    return engine.newQObject(
                obj,
                QScriptEngine::AutoOwnership,
                QScriptEngine::ExcludeDeleteLater);
#endif
}
