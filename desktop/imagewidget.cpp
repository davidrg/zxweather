#include "imagewidget.h"

#include <QList>
#include <QUrl>
#include <QDrag>
#include <QMimeData>
#include <QtDebug>
#include <QMouseEvent>
#include <QApplication>
#include <QGridLayout>

ImageWidget::ImageWidget(QWidget *parent) : QLabel(parent)
{
    setText("");
    setAcceptDrops(true);
    imageSet = false;

    // Try to maintain aspect ratio for whatever our dimensions are
    QSizePolicy policy(QSizePolicy::Preferred, QSizePolicy::Preferred);
    policy.setHeightForWidth(true);
    setSizePolicy(policy);
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

    QWidget *w = new QWidget();
    w->setAttribute(Qt::WA_DeleteOnClose);
    w->setPalette(QPalette(QColor(Qt::black)));

    QGridLayout *l = new QGridLayout(w);
    l->setMargin(0);

    ImageWidget *iw = new ImageWidget(w);
    iw->setImage(image, info);
    iw->setScaledContents(true);

    l->addItem(new QSpacerItem(1,1,QSizePolicy::Expanding, QSizePolicy::Expanding),0,0);
    l->addWidget(iw,1,0);
    l->addItem(new QSpacerItem(1,1,QSizePolicy::Expanding, QSizePolicy::Expanding),2,0);

    w->setLayout(l);

    QString title = info.title;
    if (title.isEmpty()){
        title = info.timeStamp.toString();
    } else {
        iw->setToolTip(info.timeStamp.toString());
    }

    title += " - " + info.imageSource.name;

    w->setWindowTitle(title);

    w->show();
}

// To maintain aspect ratio
int ImageWidget::heightForWidth(int width) const {
    int result = (int)(((float)image.height() / (float)image.width()) * (float)width);

    // Don't issue a height larger than the set image (it would just end up as
    // blank space anyway).
    if (imageSet && !image.isNull() && result > image.height()) {
        return this->image.height();
    }

    return result;
}
