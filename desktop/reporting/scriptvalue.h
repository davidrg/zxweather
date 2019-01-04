#ifndef SCRIPTVALUE_H
#define SCRIPTVALUE_H

#if USE_QJSENGINE
#include <QJSEngine>
#include <QJSValue>
#include <QJSValueIterator>
#else
#include <QtScript>
#endif


// This either a wrappre around QJSValue in Qt 5.6+ or an adapter to give QScriptValue
// a similar public API to QJSValue in order to minimise code differences later on.
class ScriptValue {
public:
#ifdef USE_QJSENGINE
    ScriptValue(QJSValue jsValue);
#else
    ScriptValue(QScriptValue jsValue);
#endif

    bool isError() const;

    bool hasProperty(QString prop) const;

    bool hasOwnProperty(QString prop) const;

    ScriptValue property(QString prop) const;

    ScriptValue property(uint i) const;

    bool isCallable() const;

    bool isObject() const;

    bool isArray() const;

    bool isBool() const;

#ifdef USE_QJSENGINE
    ScriptValue call(const QJSValueList &args = QJSValueList());
#else
    ScriptValue call(const QScriptValueList &args = QScriptValueList());
#endif

    QString toString() const;

    quint32 toUInt() const;

    QVariant toVariant() const;

    bool toBool() const;

#ifdef USE_QJSENGINE
    operator QJSValue() const { return value; }
#else
    operator QScriptValue() const { return value; }
#endif

private:
#ifdef USE_QJSENGINE
    QJSValue value;
#else
    QScriptValue value;
#endif
};
#endif // SCRIPTVALUE_H
