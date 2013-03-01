#ifndef CHARTWINDOW_H
#define CHARTWINDOW_H

#include <QWidget>
#include <QScopedPointer>
#include <QList>

#include "chartoptionsdialog.h"
#include "datasource/webdatasource.h"

namespace Ui {
class ChartWindow;
}

class ChartWindow : public QWidget
{
    Q_OBJECT
    
public:
    explicit ChartWindow(QList<int> columns,
                         QDateTime startTime,
                         QDateTime endTime,
                         enum ChartOptionsDialog::ChartType chartType,
                         QWidget *parent = 0);
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
    QScopedPointer<AbstractDataSource> dataSource;
    QList<int> columns;
    enum ChartOptionsDialog::ChartType chartType;
};

#endif // CHARTWINDOW_H
