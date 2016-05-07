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

private slots:
    void listItemDoubleClicked(QModelIndex index);
    void imageReady(int imageId,QImage image);
    void listItemSelectionChanged(QItemSelection selected, QItemSelection deselected);
    void treeItemSelectionChanged(QItemSelection selected, QItemSelection deselected);
private:
    Ui::ViewImagesWindow *ui;
    QScopedPointer<AbstractDataSource> dataSource;
    QScopedPointer<ImageModel> model;
    int requestedImageId;

    void loadImageForIndex(QModelIndex index);
};

#endif // VIEWIMAGESWINDOW_H
