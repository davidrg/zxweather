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
#include <QFile>
#include <QPainter>
#include <QFileDialog>
#include <QMenu>

#include "imagepropertiesdialog.h"
#include "weatherimagewindow.h"

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
    this->info.id = -1;

    videoPlayer = NULL;

    setContextMenuPolicy(Qt::CustomContextMenu);
    connect(this, SIGNAL(customContextMenuRequested(QPoint)),
            this, SLOT(contextMenuRequested(QPoint)));
}

void ImageWidget::setScaled(bool) {
    scaled = true;
}

void ImageWidget::paintEvent(QPaintEvent *event) {
    Q_UNUSED(event)

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

        // Use the supplied filename.
        this->filename = filename;
        usingCacheFile = true;

        if (image.isNull()) {
            // Perhaps this isn't an image? We'll need ImageInfo to know more
            // about this not-an-image image.

            if (!info.mimeType.isNull()) {
                // We have a MIME type!
                if (info.mimeType.startsWith("video/") || info.mimeType.startsWith("audio/")) {
                    if (info.mimeType.startsWith("video/")) {
                        setIcon(QIcon(":/icons/film-32"));
                    } else {
                        setIcon(QIcon(":/icons/audio-32"));
                    }

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
                    videoPlayer->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Expanding);
                    videoPlayer->show();
                    videoSet = true;
                } else if (info.mimeType.startsWith("audio/")) {
                    setIcon(QIcon(":/icons/audio-32"));
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

        tempFileName += QDir::cleanPath(QDir::separator() + QString("zxweather") + QDir::separator());

        if (!QDir(tempFileName).exists())
            QDir().mkpath(tempFileName);

        tempFileName = QDir::cleanPath(tempFileName + QDir::separator() + "XXXXXX.jpeg");

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

ImageInfo ImageWidget::currentImage() {
    return this->info;
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
    Q_UNUSED(event)

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

    popOut(info, image, filename);
}

void ImageWidget::popOut(ImageInfo info, QImage image, QString filename) {
    ImageWidget *iw = new ImageWidget();

    iw->setAttribute(Qt::WA_DeleteOnClose);
    if (info.mimeType.startsWith("video/")) {
        iw->setWindowIcon(QIcon(":/icons/film"));
    } else if (info.mimeType.startsWith("audio")) {
        iw->setWindowIcon(QIcon(":/icons/audio"));
    } else {
        iw->setWindowIcon(QIcon(":/icons/image"));
    }

    QFileInfo fi(filename);
    if (fi.exists() && fi.isFile()) {
        iw->setImage(image, info, filename);
    } else {
        qDebug() << "Pop-out: No cache file" << filename;
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
    // This is to ensure poped out windows containing a video get resized to an
    // appropriate size once the video has loaded.
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

// Set the height based on the images width assuming its 16:9.
// If the images (or videos) true aspect ratio is something other than 16:9 it
// will still be drawn correctly, just with black bars. We force a 16:9 ratio
// onto some images when they're excessively tall and resizing them for their width
// while maintaining aspect ratio would cause issues.
QSize ImageWidget::heightFor169Width(int width) const {
    float ratio = 9.0/16.0;

    QSize result;
    result.setWidth(width);
    result.setHeight(width * ratio);
    return result;
}

QSize ImageWidget::sizeHint() const {
    QSize s;

    if (scaled) {
        s.setWidth(width());
        s.setHeight(aspectRatioHeightForWidth(s.width()));
        if (s.height() > s.width()) {
            s = heightFor169Width(width());
        }
    } else if (videoSet && videoPlayer != NULL) {
        s = videoPlayer->sizeHint();
    } else {
        s = image.size();
    }

    return s;
}

void ImageWidget::mediaPositionChanged(qint64 time) {
    emit videoPositionChanged(time);
}

void ImageWidget::showProperties() {
    ImageWidget::showProperties(info, image, filename);
}

void ImageWidget::showProperties(ImageInfo info, QImage image, QString filename) {
    QFileInfo fileInfo(filename);
    ImagePropertiesDialog *dlg =
            new ImagePropertiesDialog(info, fileInfo.size(),
                                      image);
    dlg->setAttribute(Qt::WA_DeleteOnClose);
    dlg->show();
}

void ImageWidget::saveAs() {
    ImageWidget::saveAs(this, info, image, filename);
}

void ImageWidget::saveAs(QWidget *parent, ImageInfo info, QImage image, QString filename) {
    QFileInfo fileInfo(filename);

    QString filter;
    if (info.mimeType.startsWith("video/")) {
        filter = "Video files (*." + fileInfo.completeSuffix() + ")";
    } else if (info.mimeType.startsWith("audio/")) {
        filter = "Audio files (*." + fileInfo.completeSuffix() + ")";
    } else {
        filter = "Image files (*." + fileInfo.completeSuffix() + ")";
    }

    QString fn = QFileDialog::getSaveFileName(parent, tr("Save As..."),
        QString(), filter);

    if (info.mimeType.startsWith("image/")) {
        image.save(fn);
    } else {
        QFile::copy(filename, fn);
    }
}

void ImageWidget::weatherDataAtTime() {
    ImageWidget::weatherDataAtTime(info.id);
}

void ImageWidget::weatherDataAtTime(int imageId) {
    WeatherImageWindow *wnd = new WeatherImageWindow();
    wnd->setAttribute(Qt::WA_DeleteOnClose);
    wnd->show();
    wnd->setImage(imageId);
}

void ImageWidget::contextMenuRequested(QPoint point) {
    QMenu* menu = new QMenu(this);
    menu->setAttribute(Qt::WA_DeleteOnClose);
    QAction *act = menu->addAction("&Open in new window",
                                       this, SLOT(popOut()));
    QFont f = act->font();
    f.setBold(true);
    act->setFont(f);

    act = menu->addAction("&View weather at time",
                          this, SLOT(weatherDataAtTime()));

    menu->addSeparator();

    act = menu->addAction("&Save As...",
                    this, SLOT(saveAs()));

    menu->addSeparator();

    act = menu->addAction("&Properties",
                    this, SLOT(showProperties()));

    menu->popup(mapToGlobal(point));
}
