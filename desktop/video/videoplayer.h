#ifndef VIDEOPLAYER_H
#define VIDEOPLAYER_H

#ifndef NO_MULTIMEDIA

#include <QWidget>
#include <QMediaPlayer>
#include <QScopedPointer>

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

    void reload();

private:
    Ui::VideoPlayer *ui;
    QScopedPointer<QMediaPlayer> mediaObject;
    QSize oldSize;
    int invalidMediaRetryCount = 0;
    QString mediaFilename;
    bool autoPlay;

    void initialiseMediaPlayer();
};

#endif // NO_MULTIMEDIA

#endif // VIDEOPLAYER_H
