#ifndef SCRIPTINGENGINE_H
#define SCRIPTINGENGINE_H

#include <QObject>
#if USE_QJSENGINE
#include <QJSEngine>
#include <QJSValue>
#include <QJSValueIterator>
#else
#include <QtScript>
#endif

#include "scriptvalue.h"

#ifndef USE_QJSENGINE
#define QJSValueList QScriptValueList
#define QJSValue QScriptValue
#endif


class ScriptingEngine : public QObject
{
    Q_OBJECT
public:
    explicit ScriptingEngine(QStringList scriptFiles, QObject *parent = NULL);


    ScriptValue globalObject();

    template <typename T>
    inline QJSValue toScriptValue(const T &value)  {
        return engine.toScriptValue(value);
    }

    QJSValue newQObject(QObject* obj);

private:
    void initialiseScriptEngine();

    QStringList _scripts;

#if USE_QJSENGINE
    QJSEngine engine;
#else
    QScriptEngine engine;
#endif

};

#endif // SCRIPTINGENGINE_H
