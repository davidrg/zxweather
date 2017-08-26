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
#include <QPainter>

ImageWidget::ImageWidget(QWidget *parent) : QWidget(parent)
{
    //setText("");
    setAcceptDrops(true);
    imageSet = false;
    videoSet = false;
    usingCacheFile = false;
    scaled = false;
    videoTickInterval = 1000;
    videoControlsLocked = false;

    // Try to maintain aspect ratio for whatever our dimensions are
    QSizePolicy policy(QSizePolicy::Preferred, QSizePolicy::Preferred);
    policy.setHeightForWidth(true);
    setSizePolicy(policy);

    videoPlayer = NULL;
}

void ImageWidget::setScaled(bool) {
    scaled = true;
}

void ImageWidget::paintEvent(QPaintEvent *event) {

    // Set the background to black
    QPainter painter(this);
    painter.setBrush(QBrush(Qt::black));
    painter.drawRect(0, 0, width(), height());

    if (image.isNull()) return; // Nothing to paint

    QPixmap p = QPixmap::fromImage(image);
    if (scaled && !isIcon) {
        QPixmap scaled = p.scaled(width(), height(), Qt::KeepAspectRatio,
                                  Qt::SmoothTransformation);

        // Figure out how we're aligning the pixmap.
        if (scaled.width() < width()) {
            // horiztonal align
            int delta = width() - scaled.width();
            int offset = delta/2;
            painter.drawPixmap(offset, 0, scaled);
        } else if (scaled.height() < height()){
            // vertical align
            int delta = height() - scaled.height();
            int offset = delta/2;
            painter.drawPixmap(0, offset, scaled);
        } else {
            painter.drawPixmap(0,0,scaled);
        }
    } else {
        // horiztonal align
        int xdelta = width() - p.width();
        int xoffset = xdelta/2;

        // vertical align
        int ydelta = height() - p.height();
        int yoffset = ydelta/2;

        painter.drawPixmap(xoffset, yoffset, p);
    }
}


void ImageWidget::setPixmap(const QPixmap &pixmap) {
    imageSet = true;
    //QLabel::setPixmap(pixmap);
    image = pixmap.toImage();
    updateGeometry();
    repaint();
}

void ImageWidget::setIcon(QIcon icon) {
    isIcon = true;
    setPixmap(icon.pixmap(32,32));
}


void ImageWidget::setImage(QImage image, QString filename) {
    imageSet = false;
    videoSet = false;
    isIcon = false;

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
                    setIcon(QIcon(":/icons/film-32"));

                    // We have video! Thats sort of an image I guess.
                    qDebug() << "File is video, not image: " << filename;

                    if (videoPlayer == NULL) {
                        videoPlayer = AbstractVideoPlayer::createVideoPlayer(this);
                        connect(videoPlayer, SIGNAL(positionChanged(qint64)),
                                this, SLOT(mediaPositionChanged(qint64)));
                        connect(videoPlayer, SIGNAL(ready()),
                                this, SLOT(videoPlayerReady()));
                        this->setLayout(new QGridLayout());
                        this->layout()->setMargin(0);
                        this->layout()->addWidget(videoPlayer);
                    }
                    connect(videoPlayer, SIGNAL(sizeChanged(QSize)),
                            this, SLOT(videoSizeChanged(QSize)));

                    videoPlayer->setControlsLocked(videoControlsLocked);
                    videoPlayer->setFilename(filename);
                    videoPlayer->setTickInterval(videoTickInterval);
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
    imageSet = true;
    updateGeometry();
    repaint();
    //setPixmap(QPixmap::fromImage(image));
}

void ImageWidget::setVideoTickInterval(qint32 interval) {
    if (videoPlayer != NULL) {
        videoPlayer->setTickInterval(interval);
    }
    videoTickInterval = interval;
}

void ImageWidget::setVideoControlsEnabled(bool enabled) {
    if (enabled) {
        setVideoControlsLocked(false);
    }
    if (videoPlayer != NULL) {
        videoPlayer->setControlsEnabled(enabled);
    }
}

void ImageWidget::setVideoControlsLocked(bool locked) {
    if (videoPlayer != NULL) {
        videoPlayer->setControlsLocked(locked);
    }
    videoControlsLocked = locked;
}

void ImageWidget::videoPlayerReady() {
    emit videoReady();
}

void ImageWidget::setImage(QImage image, ImageInfo info, QString filename) {
    this->info = info;
    setImage(image, filename);
    this->setToolTip(info.timeStamp.toString());
}

void ImageWidget::mousePressEvent(QMouseEvent *event)
{
    if (!imageSet || event == NULL) {
        return;
    }

    if (event->button() == Qt::LeftButton) {
        dragStartPos = event->pos();
    }

    QWidget::mousePressEvent(event);
}

void ImageWidget::mouseMoveEvent(QMouseEvent *event) {
    if (event->buttons() & Qt::LeftButton) {
        int distance = (event->pos() - dragStartPos).manhattanLength();
        if (distance >= QApplication::startDragDistance()) {
            startDrag();
        }
    }

    QWidget::mouseMoveEvent(event);
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
    if (parentWidget() == NULL) {
        if (isMaximized()) {
            showNormal();
        } else {
           showMaximized();
        }
        repaint();
        return;
    }

//    ImageWidget *iw = new ImageWidget();


//    iw->setAttribute(Qt::WA_DeleteOnClose);
//    if (info.mimeType.startsWith("video/")) {
//        iw->setWindowIcon(QIcon(":/icons/film"));
//    } else {
//        iw->setWindowIcon(QIcon(":/icons/image"));
//    }

//    if (usingCacheFile) {
//        iw->setImage(image, info, filename);
//    } else {
//        iw->setImage(image, info);
//    }
//    iw->setScaled(true);

//    QString title = info.title;
//    if (title.isEmpty() || title.isNull()){
//        title = info.timeStamp.toString();
//    } else {
//        iw->setToolTip(info.timeStamp.toString());
//    }

//    title += " - " + info.imageSource.name;

//    iw->setWindowTitle(title);

//    iw->show();

    popOut(info, image, filename);
}

void ImageWidget::popOut(ImageInfo info, QImage image, QString filename) {
    ImageWidget *iw = new ImageWidget();

    iw->setAttribute(Qt::WA_DeleteOnClose);
    if (info.mimeType.startsWith("video/")) {
        iw->setWindowIcon(QIcon(":/icons/film"));
    } else {
        iw->setWindowIcon(QIcon(":/icons/image"));
    }

    QFileInfo fi(filename);
    if (!fi.exists() || !fi.isFile()) {
        iw->setImage(image, info, filename);
    } else {
        iw->setImage(image, info);
    }
    iw->setScaled(true);

    QString title = info.title;
    if (title.isEmpty() || title.isNull()){
        title = info.timeStamp.toString();
    } else {
        iw->setToolTip(info.timeStamp.toString());
    }

    title += " - " + info.imageSource.name;

    iw->setWindowTitle(title);

    iw->show();
}

void ImageWidget::videoSizeChanged(QSize size) {
    qDebug() << "Video size changed!";
    videoSize = size;
    //emit sizeHintChanged(sizeHint());
    updateGeometry();
    adjustSize();
}

// To maintain aspect ratio
int ImageWidget::aspectRatioHeightForWidth(int width) const {
    if (videoSet) {
        QSize videoSizeHint = videoPlayer->sizeHint();

        int result = (int)(((float)videoSizeHint.height() /
                            (float)videoSizeHint.width()) * (float)width);

        // Don't issue a height larger than the set video (it would just end up as
        // blank space anyway).
//        if (videoSet && !hasScaledContents() && result > videoSizeHint.height()) {
//            return videoSizeHint.height();
//        }

        // TODO: Extra padding for the controls, etc.

        return result;
    } else {
        int result = (int)(((float)image.height() / (float)image.width()) * (float)width);

        // Don't issue a height larger than the set image (it would just end up as
        // blank space anyway).
//        if (imageSet && !image.isNull() && !hasScaledContents()
//                && result > image.height()) {
//            return this->image.height();
//        }

        return result;
    }
}

QSize ImageWidget::sizeHint() const {
    if (videoSet && videoPlayer != NULL) {
        return videoPlayer->sizeHint();
    }
    //return QLabel::sizeHint();

    if (scaled) {
        QSize s;
        s.setWidth(width());
        s.setHeight(aspectRatioHeightForWidth(width()));
        return s;
    } else {
        return image.size();
    }
}

void ImageWidget::mediaPositionChanged(qint64 time) {
    emit videoPositionChanged(time);
}
