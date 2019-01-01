#ifndef SCRIPTRENDERWRAPPER_H
#define SCRIPTRENDERWRAPPER_H

#include <QObject>
#include <QVariant>

namespace Mustache {
    class Renderer;
    class QtVariantContext;
}

/** A simple class for wrapping up a mustache rendererer and context for access
 * from a scripting environment
 */
class ScriptRenderWrapper : public QObject
{
    Q_OBJECT
public:
    explicit ScriptRenderWrapper(Mustache::Renderer *renderer,
                                 Mustache::QtVariantContext *context,
                                 QVariant &data,
                                 QObject *parent = NULL);

public slots:
    QString renderTemplate(QString _template);
    QVariant reportData();

private:
    Mustache::Renderer* renderer;
    Mustache::QtVariantContext* context;
    QVariant &data;
};

#endif // SCRIPTRENDERWRAPPER_H
