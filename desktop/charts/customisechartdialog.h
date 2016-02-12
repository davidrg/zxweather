#ifndef CUSTOMISECHARTDIALOG_H
#define CUSTOMISECHARTDIALOG_H

#include <QDialog>
#include <QLineEdit>
#include <QComboBox>
#include <QMap>
#include <QBrush>

#include "qtcolorbutton/qtcolorbutton.h"
#include "datasource/samplecolumns.h"

#include "graphstyle.h"

namespace Ui {
class CustomiseChartDialog;
}

struct GraphSettingsWidgets {
    QLineEdit *name;
    QComboBox *lineStyle;
    QComboBox *pointStyle;
    QtColorButton *lineColour;
};

class CustomiseChartDialog : public QDialog
{
    Q_OBJECT

public:
    explicit CustomiseChartDialog(QMap<SampleColumn, GraphStyle> graphStyles,
                                  bool solarDataAvailable,
                                  bool titleEnabled,
                                  QString currentTitle,
                                  QColor titleColour,
                                  QBrush backgroundBrush,
                                  QWidget *parent = 0);
    ~CustomiseChartDialog();

    void setGraphStyles(QMap<SampleColumn, GraphStyle> graphStyles);
    QMap<SampleColumn, GraphStyle> getGraphStyles();
    bool getTitleEnabled();
    QString getTitle();
    QColor getTitleColour();
    QBrush getBackgroundBrush();

private slots:
    void AcceptDialog();

private:
    Ui::CustomiseChartDialog *ui;

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

    QMap<SampleColumn, GraphSettingsWidgets> graphSettingsWidgets;
    QMap<SampleColumn, GraphStyle> graphStyles;

    void addGraphSettingsForColumn(SampleColumn column);
    void updateGraphStyle(SampleColumn column);
};

#endif // CUSTOMISECHARTDIALOG_H
