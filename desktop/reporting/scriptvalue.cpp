#include "scriptvalue.h"

#ifdef USE_QJSENGINE
ScriptValue::ScriptValue(QJSValue jsValue): value(jsValue) {
}
#else
ScriptValue::ScriptValue(QScriptValue jsValue): value(jsValue) {

}
#endif

bool ScriptValue::isError() const {
    return value.isError();
}

bool ScriptValue::hasProperty(QString prop) const {
#if USE_QJSENGINE
    return value.hasProperty(prop);
#else
    return value.property(prop).isValid();
#endif
}

bool ScriptValue::hasOwnProperty(QString prop) const {
#ifdef USE_QJSENGINE
    return value.hasOwnProperty(prop);
#else
    return value.property(prop).isValid();
#endif
}

ScriptValue ScriptValue::property(QString prop) const {
    ScriptValue v = value.property(prop);
    return v;
}

ScriptValue ScriptValue::property(uint i) const {
    return property(QString::number(i));
}

bool ScriptValue::isCallable() const {
#ifdef USE_QJSENGINE
    return value.isCallable();
#else
    return value.isFunction();
#endif
}

bool ScriptValue::isObject() const {
    return value.isObject();
}

bool ScriptValue::isArray() const {
    return value.isArray();
}


#ifdef USE_QJSENGINE
ScriptValue ScriptValue::call(const QJSValueList &args) {

    //TODO: convert to QJSValueList
    ScriptValue result = value.call(args);
    return result;
}
#else
ScriptValue ScriptValue::call(const QScriptValueList &args) {
    //TODO: convert to QJSValueList
    ScriptValue result = value.call(QScriptValue(), args);
    return result;
}
#endif

QString ScriptValue::toString() const {
    return value.toString();
}

quint32 ScriptValue::toUInt() const {
#ifdef USE_QJSENGINE
    return value.toUInt();
#else
    return value.toUInt32();
#endif
}

QVariant ScriptValue::toVariant() const {
    return value.toVariant();
}
