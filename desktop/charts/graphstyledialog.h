#ifndef GRAPHSTYLEDIALOG_H
#define GRAPHSTYLEDIALOG_H

#include <QDialog>
#include "graphstyle.h"

namespace Ui {
class GraphStyleDialog;
}

class GraphStyleDialog : public QDialog
{
    Q_OBJECT

public:
    explicit GraphStyleDialog(GraphStyle& graphStyle, QWidget *parent = 0);
    ~GraphStyleDialog();

private slots:
    void acceptDialog();
    void restoreDefaults();

private:
    Ui::GraphStyleDialog *ui;

    GraphStyle& style;

    // This must match the order of items in the line style combo box
    enum LineStyle {
        LS_None = 0,
        LS_Line = 1,
        LS_StepLeft = 2,
        LS_StepRight = 3,
        LS_StepCenter = 4,
        LS_Impulse = 5
    };

    // This must match the order of items in the point style combo box
    enum PointStyle {
        PS_None = 0,
        PS_Dot = 1,
        PS_Cross = 2,
        PS_Plus = 3,
        PS_Circle = 4,
        PS_Disc = 5,
        PS_Square = 6,
        PS_Diamond = 7,
        PS_Star = 8,
        PS_Triangle = 9,
        PS_Triangle_Inverted = 10,
        PS_Cross_Square = 11,
        PS_Plus_Square = 12,
        PS_Cross_Circle = 13,
        PS_Plus_Circle = 14
    };
};

#endif // GRAPHSTYLEDIALOG_H
