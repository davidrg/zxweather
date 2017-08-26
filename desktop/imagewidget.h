#ifndef IMAGEWIDGET_H
#define IMAGEWIDGET_H

#include <QLabel>
#include <QImage>
#include <QString>

#include <QScopedPointer>
#include <QTemporaryFile>
#include "datasource/abstractdatasource.h"
#include "video/abstractvideoplayer.h"

class ImageWidget : public QWidget
{
    Q_OBJECT
public:
    explicit ImageWidget(QWidget *parent = 0);

    ImageInfo currentImage();

    void setImage(QImage image, QString filename=QString());
    void setImage(QImage image, ImageInfo info, QString filename=QString());
    void setIcon(QIcon icon);

    void setPixmap(const QPixmap &pixmap);

    QSize sizeHint() const;

    void popOut();

    static void popOut(ImageInfo info, QImage image, QString filename);

    void setScaled(bool);

signals:
    void videoPositionChanged(qint64 time);
    void videoReady();
//    void sizeHintChanged(QSize);

public slots:
    void setVideoTickInterval(qint32 interval);
    void setVideoControlsEnabled(bool enabled);
    void setVideoControlsLocked(bool locked);

protected:
    void mousePressEvent(QMouseEvent *event);
    void mouseMoveEvent(QMouseEvent *event);
    void mouseDoubleClickEvent(QMouseEvent *event);
    void paintEvent(QPaintEvent *);

private slots:
    void videoSizeChanged(QSize size);
    void mediaPositionChanged(qint64 time);
    void videoPlayerReady();

private:
    QString filename;
    bool imageSet;
    bool videoSet;
    bool isIcon;
    bool usingCacheFile;
    ImageInfo info;
    QImage image;
    bool scaled;
    qint32 videoTickInterval;
    bool videoControlsLocked;

    QSize videoSize;

    QPoint dragStartPos;

    QScopedPointer<QTemporaryFile> imageFile;

    AbstractVideoPlayer *videoPlayer;

    int aspectRatioHeightForWidth(int width) const;

    void startDrag();
};

#endif // IMAGEWIDGET_H
