#include "scriptingengine.h"
#include "jsconsole.h"
#include <QtDebug>

QString readTextFile(QString name, QString reportName); // defined in report.cpp

ScriptingEngine::ScriptingEngine(QStringList scriptFiles, QString reportName, QObject *parent) : QObject(parent)
{
    _reportName = _reportName;
    _scripts = scriptFiles;

    QJSValue consoleObj =  engine.newQObject(new JSConsole());
    engine.globalObject().setProperty("console", consoleObj);

    initialiseScriptEngine();
}


void ScriptingEngine::initialiseScriptEngine() {
    // Load all the script files
    QString script;
    foreach (QString file, _scripts) {
        script += readTextFile(file, _reportName) + "\n";
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
