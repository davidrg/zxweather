#include "imagestabwidget.h"
#include "imagewidget.h"

#include <QtDebug>
#include <QResizeEvent>
#include <QGridLayout>

ImagesTabWidget::ImagesTabWidget(QWidget* parent): QTabWidget(parent)
{
    setDocumentMode(true);
}

void ImagesTabWidget::hideImagery() {
    // Trash the tabs
    while (count() > 0) {
        removeTab(0);
    }
    hide();
}

void ImagesTabWidget::imageSizeHintChanged(QSize size) {
    qDebug() << "Image size changed!";
    updateGeometry();
}

void ImagesTabWidget::imageReady(ImageInfo info, QImage image, QString cacheFile) {
    qDebug() << "Processing image" << info.id << "for image source" << info.imageSource.code;

    int tabId;

    QString sourceCode = info.imageSource.code.toUpper();
    if (stationCodeTabs.contains(sourceCode)) {
        tabId = stationCodeTabs[sourceCode];
    } else {

        // We'll need to add a tab. That tabs ID will be...
        tabId = count();

        if (!tabWidgets.contains(tabId)) {
            // Create a widget for the tab
            tabWidgets[tabId] = new ImageWidget(this);
            tabWidgets[tabId]->setScaled(true);

            tabWidgets[tabId]->setSizePolicy(QSizePolicy::Maximum,
                                             QSizePolicy::Maximum);
        }

        stationCodeTabs[sourceCode] = tabId;
    }

    addTab(tabWidgets[tabId], info.imageSource.name);

    tabWidgets[tabId]->setImage(image, info, cacheFile);
    tabWidgets[tabId]->adjustSize();

    if (!isVisible()) {
        show();
    }
}
