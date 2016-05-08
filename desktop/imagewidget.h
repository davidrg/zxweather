#ifndef IMAGEWIDGET_H
#define IMAGEWIDGET_H

#include <QLabel>
#include <QImage>
#include <QString>

class ImageWidget : public QLabel
{
    Q_OBJECT
public:
    explicit ImageWidget(QWidget *parent = 0);

    void setImage(QImage image, QString filename=QString());

    void setPixmap(const QPixmap &pixmap);

protected:
    void mousePressEvent(QMouseEvent *event);

private:
    QString filename;
    bool imageSet;
};

#endif // IMAGEWIDGET_H
