#ifndef IMAGESTABWIDGET_H
#define IMAGESTABWIDGET_H

#include <QObject>
#include <QWidget>
#include <QTabWidget>
#include <QHash>
#include "datasource/imageset.h"

class ImageWidget;
class QResizeEvent;

class ImagesTabWidget : public QTabWidget
{
    Q_OBJECT

public:
    explicit ImagesTabWidget(QWidget *parent = 0);

    // Removes all image source tabs and hides the widget.
    void hideImagery();

public slots:
    void imageReady(ImageInfo info, QImage image, QString cacheFile);

private slots:

    void imageSizeHintChanged(QSize size);

private:
    QHash<int, ImageWidget*> tabWidgets;
    QHash<QString, int> stationCodeTabs;
};

#endif // IMAGESTABWIDGET_H
