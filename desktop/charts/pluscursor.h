#ifndef PLUSCURSOR_H
#define PLUSCURSOR_H

#include <QObject>

#include "plotwidget.h"

class PlusCursor: QObject
{
    Q_OBJECT
public:
    PlusCursor(PlotWidget *parent);

    bool isEnabled();
    bool isCursorEnabled() { return isEnabled();}  // TODO: Remove.

    void registerValueAxis(int type, QCPAxis *axis, bool atLeft);
    void registerKeyAxis(int type, QCPAxis *axis, bool atTop);
    void unregisterAxis(int type, QCPAxis *axis);

public slots:
    void setEnabled(bool enabled);
    void setCursorEnabled(bool enabled) {setEnabled(enabled);} // TODO: Remove.

private slots:
    void hideCursor();
    void updateCursor(QMouseEvent *event);

private:
    PlotWidget* chart;

    /** Set to true to enable the cursor, false to disable.
     */
    bool cursorEnabled;

    /** The horizontal cursor line.
     */
    QPointer<QCPItemLine> hCursor;

    /** The vertical cursor line.
     */
    QPointer<QCPItemLine> vCursor;

    /** Axis value tags for the cursor.
     * Key is an AxisType for value axes and AT_KEY+dataSetId for key axes.
     */
    QMap<int, QPointer<QCPItemText> > cursorAxisTags;
    QMap<int, QPointer<QCPAxis> > keyAxes;
    QMap<int, QPointer<QCPAxis> > valueAxes;
};

#endif // PLUSCURSOR_H
