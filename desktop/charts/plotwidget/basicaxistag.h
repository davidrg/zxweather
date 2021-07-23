#ifndef BASICAXISTAG_H
#define BASICAXISTAG_H

#include "abstractaxistag.h"

/** A basic axis tag. It goes where ever you tell it - just call setCoords.
 *
 */
class BasicAxisTag : public AbstractAxisTag
{
    Q_OBJECT

public:
    /** Constructs a BasicAxisTag
     *
     * @param keyAxis The key axis the tag will be associated with. Required if isValueTag is false, optional otherwise.
     * @param valueAxis The value axis the tag will be associated with. Required if isValueTag is true, optional otherwise.
     * @param isValueTag If the tag should be placed on the value axis rather than the key axis
     * @param arrow If the tag should be rendered with an arrow pointing towards the axis.
     * @param parent Parent object
     */
    BasicAxisTag(QCPAxis* keyAxis, QCPAxis* valueAxis, bool isValueTag, bool arrow, QObject *parent = 0);

    /** Gets the tags current coordinates.
     *
     * @return Current coordinates.
     */
    QPointF coords() const;

public slots:
    /** Sets the fixed coordinates for the tag.
     *
     * @param key Key axis value
     * @param value Value axis value
     */
    void setCoords(double key, double value);

    /** Sets the text string for the tag.
     *
     * @param text Value to show in the axis tag.
     */
    void setText(QString text);
};

#endif // BASICAXISTAG_H
