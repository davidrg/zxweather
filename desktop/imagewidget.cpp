#include "imagewidget.h"

#include <QList>
#include <QUrl>
#include <QDrag>
#include <QMimeData>

ImageWidget::ImageWidget(QWidget *parent) : QLabel(parent)
{
    setText("");
    setAcceptDrops(true);
    imageSet = false;
}

void ImageWidget::setPixmap(const QPixmap &pixmap) {
    imageSet = false;
    QLabel::setPixmap(pixmap);
}


void ImageWidget::setImage(QImage image, QString filename) {

    if (filename.isNull() || filename.isEmpty()) {
        // Save the image somewhere temporarily to enable drag-drop operations
        imageFile.reset(new QTemporaryFile("XXXXXX.jpeg"));
        image.save(imageFile.data());
        imageFile->close();
        this->filename = imageFile->fileName();
    } else {
        this->filename = filename;
    }

    setPixmap(QPixmap::fromImage(image));
    imageSet = true;
}

void ImageWidget::setImage(QImage image, ImageInfo info, QString filename) {
    setImage(image, filename);
    this->info = info;
    this->setToolTip(info.timeStamp.toString());
}

void ImageWidget::mousePressEvent(QMouseEvent *event)
{
    if (!imageSet) {
        return;
    }

    QList<QUrl> urls;
    urls << QUrl::fromLocalFile(this->filename);

    QMimeData *mimeData = new QMimeData;
    mimeData->setUrls(urls);

    QDrag *drag = new QDrag(this);
    drag->setMimeData(mimeData);

    drag->exec(Qt::CopyAction, Qt::CopyAction);
}
