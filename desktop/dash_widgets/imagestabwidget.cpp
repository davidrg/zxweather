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

    foreach (ImageWidget *w, tabWidgets) {
        delete w;
    }
    tabWidgets.clear();
    stationCodeTabs.clear();

    hide();
}

void ImagesTabWidget::imageSizeHintChanged(QSize size) {
    Q_UNUSED(size)

    qDebug() << "Image size changed!";
    adjustSize();
    updateGeometry();
    qDebug() << sizeHint();
}

void ImagesTabWidget::imageReady(ImageInfo info, QImage image, QString cacheFile) {
    qDebug() << "Processing image" << info.id << "for image source" << info.imageSource.code;

    int tabId;

    QString sourceCode = info.imageSource.code.toUpper();
    if (stationCodeTabs.contains(sourceCode)) {
        tabId = stationCodeTabs[sourceCode];

        ImageInfo current = tabWidgets[tabId]->currentImage();
        if (!current.timeStamp.isNull()) {
            if (imageLessThan(info, current)) {
                // new image has lower sort order than the image currently being
                // displayed - either its older or it has a lower ordered image
                // type. Most likely a less interesting APT enhancement than the
                // one currently displayed.
                return;
            }
        }
    } else {

        // We'll need to add a tab. That tabs ID will be...
        tabId = count();

        // Create a widget for the tab
        tabWidgets[tabId] = new ImageWidget(this);
        tabWidgets[tabId]->setScaled(true);

        QSizePolicy pol(QSizePolicy::MinimumExpanding, QSizePolicy::MinimumExpanding);
                    pol.setHeightForWidth(true);
        tabWidgets[tabId]->setSizePolicy(pol);

        tabWidgets[tabId]->setSizePolicy(QSizePolicy::Expanding,
                                         QSizePolicy::Expanding);

        stationCodeTabs[sourceCode] = tabId;

        addTab(tabWidgets[tabId], info.imageSource.name);
    }

    tabWidgets[tabId]->setImage(image, info, cacheFile);

    // Make sure we're visible now that we've got an image!
    show();
}
