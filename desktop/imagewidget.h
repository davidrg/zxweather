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

    void setImage(QImage image, QString filename=QString());
    void setImage(QImage image, ImageInfo info, QString filename=QString());

    void setPixmap(const QPixmap &pixmap);

    QSize sizeHint() const;

    void popOut();

    void setScaled(bool);

//signals:
//    void sizeHintChanged(QSize);
protected:
    void mousePressEvent(QMouseEvent *event);
    void mouseMoveEvent(QMouseEvent *event);
    void mouseDoubleClickEvent(QMouseEvent *event);
    void paintEvent(QPaintEvent *);

private slots:
    void videoSizeChanged(QSize size);

private:
    QString filename;
    bool imageSet;
    bool videoSet;
    bool usingCacheFile;
    ImageInfo info;
    QImage image;
    bool scaled;

    QSize videoSize;

    QPoint dragStartPos;

    QScopedPointer<QTemporaryFile> imageFile;

    AbstractVideoPlayer *videoPlayer;

    int aspectRatioHeightForWidth(int width) const;

    void startDrag();
};

#endif // IMAGEWIDGET_H
