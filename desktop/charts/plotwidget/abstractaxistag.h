#ifndef ABSTRACTAXISTAG_H
#define ABSTRACTAXISTAG_H

#include <QObject>
#include <QPointer>


class QCPAxis;
class QCPItemLine;
class QCPItemText;
class GraphStyle;


/** Tags for chart axes!
 *
 * Use BasicAxisTag if you want to just put a tag on particular coordinates.
 * Use TracingAxisTag if you want the tag to follow a QCPItemTracer.
 */
class AbstractAxisTag: public QObject
{
    Q_OBJECT
public:
    /** Constructs a new AbstractAxisTag.
     *
     *  Note that value axis tags may not align correctly if both a key and a value axis are not
     *  supplied.
     *
     * @param keyAxis Key axis to be associated with this axis tag. Required if the tag is against the key axis, optional otherwise.
     * @param valueAxis Value axis to be associated with this axis tag. Required if the tag is against the value axis, optional otherwise.
     * @param onValueAxis If the tag is against the value axis rather than key axis
     * @param arrow If the axis tag should include an arrow pointing to the axis
     * @param parent Parent object
     */
    AbstractAxisTag(QCPAxis* keyAxis, QCPAxis* valueAxis, bool onValueAxis, bool arrow, QObject *parent = 0);
    ~AbstractAxisTag();

    /** Sets the tags style according to the specified GraphStyle
     *
     * @param style GraphStyle to draw the axis tag in
     */
    void setStyle(const GraphStyle &style);

    /** Sets the pen to use when drawing the tag
     *
     * @param pen Pen to use
     */
    void setPen(const QPen &pen);

    /** Gets the axis tags font
     *
     * @return Font
     */
    QFont font();

    /** Gets the axis tags current text
     *
     * @return Current text
     */
    QString text();

public slots:
    /** Sets the visibiltiy of the axis tag. This does not queue a replot - you'll
     *  likely want to do that yourself.
     *
     * @param visible If the tag should be visible or not.
     */
    void setVisible(bool visible);

protected:
    QPointer<QCPItemLine> arrow;
    QPointer<QCPItemText> label;
    QPointer<QCPAxis> keyAxis;
    QPointer<QCPAxis> valueAxis;
    bool onValueAxis;

    void setAxes(QCPAxis *keyAxis, QCPAxis *valueAxis);
    QCPAxis* axis();

private:
    char format;
    int precision;
};

#endif // ABSTRACTAXISTAG_H
