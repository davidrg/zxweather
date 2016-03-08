#ifndef VIEWDATASETWINDOW_H
#define VIEWDATASETWINDOW_H

#include <QMainWindow>
#include <QScopedPointer>
#include <QShortcut>
#include "datasource/samplecolumns.h"

#include "datasource/abstractdatasource.h"

namespace Ui {
class ViewDataSetWindow;
}

class ViewDataSetWindow : public QMainWindow
{
    Q_OBJECT

public:
    explicit ViewDataSetWindow(DataSet dataSet, QWidget *parent = 0);
    ~ViewDataSetWindow();

public slots:
    virtual void show();
    void copySelection();

private slots:
    void samplesReady(SampleSet samples);
    void samplesFailed(QString message);

private:
    Ui::ViewDataSetWindow *ui;

    DataSet dataSet;
    QShortcut *copy;
    QScopedPointer<AbstractDataSource> dataSource;
};

#endif // VIEWDATASETWINDOW_H
