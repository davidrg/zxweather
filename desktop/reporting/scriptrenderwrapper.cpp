#include "scriptrenderwrapper.h"
#include "reporting/qt-mustache/mustache.h"

ScriptRenderWrapper::ScriptRenderWrapper(Mustache::Renderer *renderer,
                                         Mustache::QtVariantContext *context,
                                         QVariant &data,
                                         QObject *parent) : QObject(parent), data(data)
{
    this->renderer = renderer;
    this->context = context;
}

QString ScriptRenderWrapper::renderTemplate(QString _template) {
    return this->renderer->render(_template, context);
}

QVariant ScriptRenderWrapper::reportData() {
    return data;
}
