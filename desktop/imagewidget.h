#ifndef IMAGEWIDGET_H
#define IMAGEWIDGET_H

#include <QLabel>
#include <QImage>
#include <QString>

#include <QScopedPointer>
#include <QTemporaryFile>
#include "datasource/abstractdatasource.h"

class ImageWidget : public QLabel
{
    Q_OBJECT
public:
    explicit ImageWidget(QWidget *parent = 0);

    void setImage(QImage image, QString filename=QString());
    void setImage(QImage image, ImageInfo info, QString filename=QString());

    void setPixmap(const QPixmap &pixmap);

protected:
    void mousePressEvent(QMouseEvent *event);

private:
    QString filename;
    bool imageSet;
    ImageInfo info;

    QScopedPointer<QTemporaryFile> imageFile;
};

#endif // IMAGEWIDGET_H
