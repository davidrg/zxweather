#ifndef VIEWIMAGESWINDOW_H
#define VIEWIMAGESWINDOW_H

#include <QMainWindow>
#include <QScopedPointer>
#include <QItemSelection>
#include <QDateTime>

#include "datasource/abstractdatasource.h"
#include "imagemodel.h"

namespace Ui {
class ViewImagesWindow;
}

class QLabel;

class ViewImagesWindow : public QMainWindow
{
    Q_OBJECT

public:
    explicit ViewImagesWindow(QDate atDate = QDate(), QWidget *parent = 0);
    ~ViewImagesWindow();

public slots:
    void newImage(NewImageInfo info);

protected:
    void closeEvent(QCloseEvent *event);

private slots:
    void listItemDoubleClicked(QModelIndex index);
    void detailItemDoubleClicked(QModelIndex index);
    void treeItemDoubleClicked(QModelIndex index);

    void listItemSelectionChanged(QItemSelection selected, QItemSelection deselected);
    void detailItemSelectionChanged(QItemSelection selected, QItemSelection deselected);
    void treeItemSelectionChanged(QItemSelection selected, QItemSelection deselected);

    void hSplitterMoved(int, int);
    void vSplitterMoved(int, int);

    void listItemContextMenu(const QPoint& point);
    void detailItemContextMenu(const QPoint& point);
    void treeItemContextMenu(const QPoint& point);

    void expandNow();

    // Context menu
    void openImageInWindow();
    void saveImageAs();
    void viewWeather();
    void properties();
    void copy();
    void openItem();
    void expandRecursively();
    void collapseRecursively();

    void setViewModeMenuHandler();

    void showHidePreviewPane(bool show);
    void showHideTreePane(bool show);

    void navigateUp();

    void setViewIndex(QModelIndex index);

    void lazyLoadingComplete(QModelIndex index);
    void imageReady(QModelIndex index);

#ifdef QT_DEBUG
    void testFindIndex();
#endif

private:
    Ui::ViewImagesWindow *ui;
    QScopedPointer<AbstractDataSource> dataSource;
    QScopedPointer<ImageModel> model;
    QModelIndex currentImageIndex;
    bool imageLoaded;

    QDate onLoadExpandDate;

    // Status bar
    QLabel *location;
    QLabel *itemCount;

    typedef enum  {
        CMS_Tree = 1,
        CMS_List = 2,
        CMS_Detail = 3,
        CMS_Toolbar = 4
    } ContextMenuSource;

    typedef enum {
        VIWM_Default = 0,
        VIWM_Thumbnails = 1,
        VIWM_Icons = 2,
        VIWM_SmallIcons = 3,
        VIWM_Detail = 4,
        VIWM_SmallThumbnails = 5
    } ViewImagesWindowViewMode;

    ViewImagesWindowViewMode currentViewMode;

    void setViewMode(ViewImagesWindowViewMode mode);

    void loadImageForIndex(QModelIndex index);
    void contextMenu(QPoint point, QModelIndex idx, ContextMenuSource source);
    void expandDate(QDate date, bool expandDate);
    void updateLocation(QModelIndex index);
    QString getCurrentLocation(QModelIndex index);
    void updateItemCount();
    void updateToolbarStatus(QModelIndex index);

    QModelIndex normaliseIndexColumn(QModelIndex index);
};

#endif // VIEWIMAGESWINDOW_H
