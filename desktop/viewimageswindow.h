#ifndef VIEWIMAGESWINDOW_H
#define VIEWIMAGESWINDOW_H

#include <QMainWindow>
#include <QScopedPointer>
#include <QItemSelection>

#include "datasource/abstractdatasource.h"
#include "imagemodel.h"

namespace Ui {
class ViewImagesWindow;
}

class ViewImagesWindow : public QMainWindow
{
    Q_OBJECT

public:
    explicit ViewImagesWindow(QWidget *parent = 0);
    ~ViewImagesWindow();

protected:
    void closeEvent(QCloseEvent *event);

private slots:
    void listItemDoubleClicked(QModelIndex index);
    void listItemSelectionChanged(QItemSelection selected, QItemSelection deselected);
    void treeItemSelectionChanged(QItemSelection selected, QItemSelection deselected);
    void hSplitterMoved(int, int);
    void vSplitterMoved(int, int);

private:
    Ui::ViewImagesWindow *ui;
    QScopedPointer<AbstractDataSource> dataSource;
    QScopedPointer<ImageModel> model;

    void loadImageForIndex(QModelIndex index);
};

#endif // VIEWIMAGESWINDOW_H
