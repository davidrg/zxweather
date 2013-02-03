#ifndef CHARTWINDOW_H
#define CHARTWINDOW_H

#include <QDialog>
#include <QScopedPointer>
#include <QList>

#include "chartoptionsdialog.h"
#include "datasource/webdatasource.h"

namespace Ui {
class ChartWindow;
}

class ChartWindow : public QDialog
{
    Q_OBJECT
    
public:
    explicit ChartWindow(QWidget *parent = 0);
    ~ChartWindow();
    
private slots:
    void refresh();
    void samplesReady(SampleSet samples);

    // chart slots
    void mousePress();
    void mouseWheel();
    void selectionChanged();

    // Save As slot
    void save();

private:
    Ui::ChartWindow *ui;
    QScopedPointer<WebDataSource> dataSource;
    QList<int> columns;
    enum ChartOptionsDialog::ChartType chartType;
};

#endif // CHARTWINDOW_H
