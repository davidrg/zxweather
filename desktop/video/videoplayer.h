#ifndef VIDEOPLAYER_H
#define VIDEOPLAYER_H

#include <QWidget>
#include <QMediaPlayer>

#include "abstractvideoplayer.h"

namespace Ui {
class VideoPlayer;
}

/** Video player for Qt 5.x using QtMultimedia
 *
 */
class VideoPlayer : public AbstractVideoPlayer
{
    Q_OBJECT

public:
    explicit VideoPlayer(QWidget *parent = 0);
    ~VideoPlayer();

    void setFilename(QString filename);

    QSize videoSize();

    QSize sizeHint() const;

    bool controlsEnabled() const;

public slots:
    void play();
    void pause();
    void stop();
    void setTickInterval(qint32 interval);
    void setControlsEnabled(bool enabled);

private slots:
    void finished();
    void updateTime();
    void mediaStatusChanged(QMediaPlayer::MediaStatus newStatus);
    void stateChanged(QMediaPlayer::State newState);

    void setStatus(QString status);
    void mediaError(QMediaPlayer::Error error);
    void mediaPositionChanged(qint64 pos);

private:
    Ui::VideoPlayer *ui;
    QMediaPlayer mediaObject;
    QSize oldSize;
};

#endif // VIDEOPLAYER_H
