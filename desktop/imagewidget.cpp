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
    this->filename = filename;
    setPixmap(QPixmap::fromImage(image));
    imageSet = true;
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
