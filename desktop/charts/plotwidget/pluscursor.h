#ifndef PLUSCURSOR_H
#define PLUSCURSOR_H

#include <QObject>
#include <QMap>
#include <QPointer>

class BasicAxisTag;
class PlotWidget;
class QCPAxis;
class QCPItemLine;
class QCPAxisRect;
class QMouseEvent;

/** Draws a cursor under the mouse pointer with lines extending to all sides
 * of the plot. Where the line intersects with an axis a tag is drawn showing the
 * value on that axis.
 */
class PlusCursor : public QObject
{
    Q_OBJECT
public:
    explicit PlusCursor(PlotWidget *plotWidget);

    /** If the cursor is currently enabled.
     *
     * @return If enabled
     */
    bool isEnabled() { return enabled; }

public slots:
    /** Sets the cursor visibility
     *
     * @param enabled Set visibility
     */
    void setEnabled(bool enabled);

    /** Temporarily hides the cursor. It will reappear when the mouse pointer next
     *  moves within an axis rect.
     *
     * This can be called when transformations are been applied to the plot which may
     * invalidate the cursors current position (eg, zooming) to clear the cursors
     * current position and hide it until the mouse is moved again.
     */
    void hideCursor();

private slots:
    void mouseMove(QMouseEvent *event);
    void mouseLeave(QEvent *event);

private:
    bool setup(QCPAxisRect *rect);
    void cleanup();
    QCPAxis* getVisibleKeyAxis(QCPAxisRect *rect);
    QCPAxis* getVisibleValueAxis(QCPAxisRect *rect);

    bool enabled;

    /** The horizontal cursor line.
     */
    QPointer<QCPItemLine> hCursor;

    /** The vertical cursor line.
     */
    QPointer<QCPItemLine> vCursor;

    /** The axis rect the cursor is currently in
     */
    QPointer<QCPAxisRect> currentAxisRect;

    QMap<int, QPointer<BasicAxisTag> > cursorAxisTags;
    QMap<int, QCPAxis*> keyAxes;
    QMap<int, QCPAxis*> valueAxes;

    PlotWidget * chart;
};

#endif // PLUSCURSOR_H
