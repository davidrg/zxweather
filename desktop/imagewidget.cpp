#include "imagewidget.h"

#include <QList>
#include <QUrl>
#include <QDrag>
#include <QMimeData>
#include <QtDebug>
#include <QMouseEvent>
#include <QApplication>

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

    this->image = image;
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

    if (event->button() == Qt::LeftButton) {
        dragStartPos = event->pos();
    }

    QLabel::mousePressEvent(event);
}

void ImageWidget::mouseMoveEvent(QMouseEvent *event) {
    if (event->buttons() & Qt::LeftButton) {
        int distance = (event->pos() - dragStartPos).manhattanLength();
        if (distance >= QApplication::startDragDistance()) {
            startDrag();
        }
    }

    QLabel::mouseMoveEvent(event);
}

void ImageWidget::startDrag() {
    QList<QUrl> urls;
    urls << QUrl::fromLocalFile(this->filename);

    QMimeData *mimeData = new QMimeData;
    mimeData->setUrls(urls);

    QDrag *drag = new QDrag(this);
    drag->setMimeData(mimeData);

    drag->exec(Qt::CopyAction, Qt::CopyAction);
}

void ImageWidget::mouseDoubleClickEvent(QMouseEvent *event) {
    if (!imageSet) {
        return; // Nothing to do
    }

    ImageWidget *w = new ImageWidget();
    w->setAttribute(Qt::WA_DeleteOnClose);
    w->setImage(image, info, filename);
    w->setScaledContents(true);

    QString title = info.title;
    if (title.isEmpty()){
        title = info.timeStamp.toString();
    } else {
        w->setToolTip(info.timeStamp.toString());
    }

    title += " - " + info.imageSource.name;

    w->setWindowTitle(title);

    w->show();
}
