#include "imagewidget.h"

#include <QList>
#include <QUrl>
#include <QDrag>
#include <QMimeData>
#include <QtDebug>
#include <QMouseEvent>
#include <QApplication>
#include <QGridLayout>
#include <QIcon>
#include <QFileInfo>
#include <QDesktopServices>
#include <QDir>

ImageWidget::ImageWidget(QWidget *parent) : QLabel(parent)
{
    setText("");
    setAcceptDrops(true);
    imageSet = false;
    usingCacheFile = false;

    // Try to maintain aspect ratio for whatever our dimensions are
    QSizePolicy policy(QSizePolicy::Preferred, QSizePolicy::Preferred);
    policy.setHeightForWidth(true);
    setSizePolicy(policy);

    videoPlayer = NULL;
}

void ImageWidget::setPixmap(const QPixmap &pixmap) {
    imageSet = true;
    QLabel::setPixmap(pixmap);
    repaint();
}


void ImageWidget::setImage(QImage image, QString filename) {
    imageSet = false;
    videoSet = false;

    bool fileOk = true;
    if (filename.isNull() || filename.isEmpty()) {
        fileOk = false; // No filename specified
    }

    QFileInfo fi(filename);
    if (!fi.exists() || !fi.isFile()) {
        fileOk = false; // File missing or not a file
    }

    if (fileOk) {

        if (this->filename == filename) {
            // Same file. No need to reload
            return;
        }

        // Use the supplied filename.
        this->filename = filename;
        usingCacheFile = true;

        if (image.isNull()) {
            // Perhaps this isn't an image? We'll need ImageInfo to know more
            // about this not-an-image image.

            if (!info.mimeType.isNull()) {
                // We have a MIME type!
                if (info.mimeType.startsWith("video/")) {
                    setPixmap(QPixmap(":/icons/film-32"));

                    // We have video! Thats sort of an image I guess.
                    qDebug() << "File is video, not image: " << filename;

                    if (videoPlayer == NULL) {
                        videoPlayer = AbstractVideoPlayer::createVideoPlayer(this);
                        this->setLayout(new QGridLayout());
                        this->layout()->setMargin(0);
                        this->layout()->addWidget(videoPlayer);
                    }
                    connect(videoPlayer, SIGNAL(sizeChanged(QSize)),
                            this, SLOT(videoSizeChanged(QSize)));

                    videoPlayer->setFilename(filename);
                    videoPlayer->show();
                    videoSet = true;

                }
            }
        } else {
            if (videoPlayer != NULL) {
                videoPlayer->stop();
                videoPlayer->hide();
            }
        }

    } else {
#if QT_VERSION >= 0x050000
        QString tempFileName = QStandardPaths::writableLocation(
                    QStandardPaths::CacheLocation);
#else
        QString tempFileName = QDesktopServices::storageLocation(
                    QDesktopServices::TempLocation);
#endif

        tempFileName += "/zxweather/";

        if (!QDir(tempFileName).exists())
            QDir().mkpath(tempFileName);

        tempFileName += "XXXXXX.jpeg";

        // Save the image somewhere temporarily to enable drag-drop operations
        imageFile.reset(new QTemporaryFile(tempFileName));
        image.save(imageFile.data());
        imageFile->close();
        this->filename = imageFile->fileName();
    }

    this->image = image;
    setPixmap(QPixmap::fromImage(image));
}

void ImageWidget::setImage(QImage image, ImageInfo info, QString filename) {
    this->info = info;
    setImage(image, filename);
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

    popOut();
}

void ImageWidget::popOut() {

    // If we're already a pop-up window, instead of opening another popup,
    // switch between maximized and normal.
    if (parentWidget()->isWindow()) {
        if (parentWidget()->isMaximized()) {
            parentWidget()->showNormal();
        } else {
            parentWidget()->showMaximized();
        }
        parentWidget()->repaint();
        return;
    }

    QWidget *w = new QWidget();
    w->setAttribute(Qt::WA_DeleteOnClose);
    w->setPalette(QPalette(QColor(Qt::black)));

    if (info.mimeType.startsWith("video/")) {
        w->setWindowIcon(QIcon(":/icons/film"));
    } else {
        w->setWindowIcon(QIcon(":/icons/image"));
    }

    QGridLayout *l = new QGridLayout(w);
    l->setMargin(0);

    ImageWidget *iw = new ImageWidget(w);
    if (usingCacheFile) {
        iw->setImage(image, info, filename);
    } else {
        iw->setImage(image, info);
    }
    iw->setScaledContents(true);

    // When displaying video we don't need the spacers - the video player
    // scales the display area as appropriate while maintaining aspect ratio
    if (!videoSet) {
        l->addItem(new QSpacerItem(1,1,QSizePolicy::Expanding, QSizePolicy::Expanding),0,0);
    }
    l->addWidget(iw,1,0);
    if (!videoSet) {
        l->addItem(new QSpacerItem(1,1,QSizePolicy::Expanding, QSizePolicy::Expanding),2,0);
    }

    w->setLayout(l);

    QString title = info.title;
    if (title.isEmpty() || title.isNull()){
        title = info.timeStamp.toString();
    } else {
        iw->setToolTip(info.timeStamp.toString());
    }

    title += " - " + info.imageSource.name;

    w->setWindowTitle(title);

    w->show();
    w->repaint();
}

void ImageWidget::videoSizeChanged(QSize size) {
    videoSize = size;
    emit sizeHintChanged(sizeHint());
    updateGeometry();
}

// To maintain aspect ratio
int ImageWidget::heightForWidth(int width) const {
    if (videoSet) {
        QSize videoSizeHint = videoPlayer->sizeHint();

        int result = (int)(((float)videoSizeHint.height() /
                            (float)videoSizeHint.width()) * (float)width);

        // Don't issue a height larger than the set video (it would just end up as
        // blank space anyway).
        if (videoSet && !hasScaledContents() && result > videoSizeHint.height()) {
            return videoSizeHint.height();
        }

        // TODO: Extra padding for the controls, etc.

        return result;
    } else {
        int result = (int)(((float)image.height() / (float)image.width()) * (float)width);

        // Don't issue a height larger than the set image (it would just end up as
        // blank space anyway).
        if (imageSet && !image.isNull() && !hasScaledContents()
                && result > image.height()) {
            return this->image.height();
        }

        return result;
    }
}

QSize ImageWidget::sizeHint() const {
    if (videoSet) {
        return videoPlayer->sizeHint();
    }
    return QLabel::sizeHint();
}
