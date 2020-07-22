#ifndef NO_MULTIMEDIA

#include "videoplayer.h"
#include "ui_videoplayer.h"

#include <QTime>
#include <QStyle>
#include <QTimer>

VideoPlayer::VideoPlayer(QWidget *parent) :
    AbstractVideoPlayer(parent),
    ui(new Ui::VideoPlayer)
{
    ui->setupUi(this);

    connect(ui->tbPlay, SIGNAL(pressed()),
            this, SLOT(play()));
    connect(ui->tbPause, SIGNAL(pressed()),
            this, SLOT(pause()));
    connect(ui->tbStop, SIGNAL(pressed()),
            this, SLOT(stop()));

    ui->tbPlay->setText("");
    ui->tbPlay->setIcon(style()->standardIcon(QStyle::SP_MediaPlay));
    ui->tbPause->setText("");
    ui->tbPause->setIcon(style()->standardIcon(QStyle::SP_MediaPause));
    ui->tbStop->setText("");
    ui->tbStop->setIcon(style()->standardIcon(QStyle::SP_MediaStop));
    ui->frame->setPalette(QApplication::palette());
    ui->frame->setAutoFillBackground(true);

    initialiseMediaPlayer();
    autoPlay = false;
}

VideoPlayer::~VideoPlayer()
{
    delete ui;
}

void VideoPlayer::initialiseMediaPlayer() {
    mediaObject.reset(new QMediaPlayer());

    mediaObject->setVideoOutput(ui->player);

    setTickInterval(1000);

    connect(mediaObject.data(), SIGNAL(durationChanged(qint64)),
            this, SLOT(updateTime()));
    connect(mediaObject.data(), SIGNAL(positionChanged(qint64)),
            this, SLOT(updateTime()));
    connect(mediaObject.data(), SIGNAL(mediaStatusChanged(QMediaPlayer::MediaStatus)),
            this, SLOT(mediaStatusChanged(QMediaPlayer::MediaStatus)));
    connect(mediaObject.data(), SIGNAL(stateChanged(QMediaPlayer::State)),
            this, SLOT(stateChanged(QMediaPlayer::State)));
    connect(mediaObject.data(), SIGNAL(error(QMediaPlayer::Error)),
            this, SLOT(mediaError(QMediaPlayer::Error)));
    connect(mediaObject.data(), SIGNAL(positionChanged(qint64)),
            this, SLOT(mediaPositionChanged(qint64)));

    setControlsEnabled(false);
}

void VideoPlayer::setTickInterval(qint32 interval) {
    mediaObject->setNotifyInterval(interval);
}

void VideoPlayer::play() {
    qDebug() << "> PLAY <";
    mediaObject->play();
    ui->tbPlay->setChecked(true);
}

void VideoPlayer::pause() {
    qDebug() << "> PAUSE <";
    mediaObject->pause();
    ui->tbPause->setChecked(true);
}

void VideoPlayer::stop() {
    qDebug() << "> STOP <";
    mediaObject->stop();
    ui->tbStop->setChecked(true);
}

void VideoPlayer::setFilename(QString filename) {
    qDebug() << "> LOAD <";
    mediaFilename = filename;
    mediaObject->setMedia(QUrl::fromLocalFile(filename));

    if (!autoPlay) {
        setStatus(tr("Loading..."));
    }
}

void VideoPlayer::finished() {
    qDebug() << "Finished!";
    ui->tbStop->setChecked(true);
}

void VideoPlayer::updateTime() {
    ui->lTime->setText(timeString(mediaObject->duration(),
                                  mediaObject->position()));
}

bool VideoPlayer::controlsEnabled() const {
    return ui->tbPause->isEnabled();
}

void VideoPlayer::setControlsEnabled(bool enabled) {
    if (mediaObject->mediaStatus() != QMediaPlayer::LoadedMedia) {
        return; // Not valid to enable controls yet.
    }

    ui->tbPause->setEnabled(enabled);
    ui->tbPlay->setEnabled(enabled);
    ui->tbStop->setEnabled(enabled);
}

void VideoPlayer::setStatus(QString status) {
    qDebug() << "setStatus: " << status;
    ui->lStatus->setText(status);
}

QSize VideoPlayer::videoSize() {
    if (mediaObject->mediaStatus() == QMediaPlayer::LoadingMedia) {
        return QSize();
    }
    return ui->player->sizeHint();
}

void VideoPlayer::mediaStatusChanged(QMediaPlayer::MediaStatus newStatus) {


    switch(mediaObject->state()) {
    case QMediaPlayer::StoppedState:
        qDebug() << "mediaStatusChanged! State is: Stopped";
        break;
    case QMediaPlayer::PlayingState:
        qDebug() << "mediaStatusChanged! State is: Playing";
        break;
    case QMediaPlayer::PausedState:
        qDebug() << "mediaStatusChanged! State is: Paused";
        break;
    default:
        qDebug() << "mediaStatusChanged!";
    }

    qDebug() << "Position:" << mediaObject->position();


    switch (newStatus) {
    case QMediaPlayer::LoadingMedia:
        qDebug() << "mediaStatus: Loading Media";
        if (!autoPlay) {
            setStatus(tr("Loading..."));
        }
        setControlsEnabled(false);
        break;
    case QMediaPlayer::EndOfMedia:
        qDebug() << "mediaStatus: End Of Media";
        setStatus(tr("Paused"));
        ui->tbPause->setChecked(true);

        /* What is this?
         * There seems to be a random chance of the QMediaPlayer instance starting
         * off broken. There isn't any reliable way of reproducing the problem
         * and there isn't any obvious cause. Its been observed on multiple
         * computers and under multiple Qt versions. Perhaps caused by a codec
         * issue?
         *
         * The symptoms are: The media loads fine but as soon as you hit play we
         * get the EndOfMedia status nearly immediately. The players current
         * position at this point is only a few seconds in - nowhere near the
         * actual end of media. Hitting play again will cause another EndOfMedia
         * status this time actually at the end of media. The player doesn't
         * go back to the beginning so hitting play again just gets another
         * EndOfMedia.
         *
         * Further clicking of Play/Pause/Stop randomly for a while may eventually
         * make the video play but there doesn't appear to be any reliable sequence
         * to make it behave.
         *
         * By far the easiest option when we notice this problem is just to delete
         * and recreate the QMediaPlayer. Chances are the new one will work fine
         * and play the video properly first try. So if we get an EndOfMedia
         * status that isn't the real end of the media thats what we do!
         */
        if (mediaObject->position() != mediaObject->duration()) {
            qWarning() << "Faulty end of media position!";
            setStatus(tr("Media Player Failure - Reloading..."));

            initialiseMediaPlayer();
            autoPlay = true;

            setFilename(mediaFilename);
        }

        break;
    case QMediaPlayer::LoadedMedia:
        qDebug() << "mediaStatus: Loaded Media";
        setStatus(tr("Stopped"));
        if (!controlsLocked) {
            setControlsEnabled(true);
        }
        emit ready();
        if (autoPlay) {
            autoPlay = false;
            play();
        }
        break;
    case QMediaPlayer::BufferingMedia:
        qDebug() << "mediaStatus: Buffering Media";
//        setStatus("Buffering");
        break;
    case QMediaPlayer::StalledMedia:
        qDebug() << "mediaStatus: Stalled Media";
//        setStatus("Stalled");
        break;
    case QMediaPlayer::BufferedMedia:
        qDebug() << "mediaStatus: Buffered Media";
//        setStatus("Buffered");
        break;
    case QMediaPlayer::InvalidMedia:
        qDebug() << "mediaStatus: Invalid Media";
        setStatus(tr("Invalid Media"));
        setControlsEnabled(false);

        invalidMediaRetryCount++;
        if (invalidMediaRetryCount > 2) {
            qDebug() << "Failed to load media after 2 attempts";
            invalidMediaRetryCount = 0;
            return;
        } else {
            // This might have been caused by trying to load the video before
            // its been fully written to disk. Try re-loading it in a second -
            // by then it should be done.
            qDebug() << "Got invalid media status while loading file"
                     << mediaFilename << "- attempting to reload.";
            QTimer::singleShot(1000,this, SLOT(reload()));
        }
        break;
    case QMediaPlayer::UnknownMediaStatus:
        qDebug() << "mediaStatus: Unknown Media Status";
    case QMediaPlayer::NoMedia:
    default:
        qDebug() << "mediaStatus: No Media";
        setStatus(tr("No Media"));
        setControlsEnabled(false);
    }

    if (newStatus != QMediaPlayer::LoadingMedia) {
        QSize size = videoSize();
        if (oldSize.width() != size.width() ||
                oldSize.height() != size.height()) {
            oldSize = size;
            qDebug() << "Size changed: " << size;
            updateGeometry();
            emit sizeChanged(size);
        }
    }
}

void VideoPlayer::stateChanged(QMediaPlayer::State newState) {
    qDebug() << "State changed";
    switch (newState) {
    case QMediaPlayer::PlayingState:
        qDebug() << "stateChanged: Playing";
        setStatus(tr("Playing"));
        break;
    case QMediaPlayer::PausedState:
        qDebug() << "stateChanged: Paused";
        setStatus(tr("Paused"));
        break;
    case QMediaPlayer::StoppedState:
        qDebug() << "stateChanged: Stopped";
        // We handle this via media status changed so we can handle
        // stopped vs end of media.
    default:
        break; // do nothing
    }
}

void VideoPlayer::mediaError(QMediaPlayer::Error /*error*/) {
    setStatus(QString(tr("Error: %1")).arg(mediaObject->errorString()));
}

QSize VideoPlayer::sizeHint() const {
    int frameHeight = ui->frame->height();
    int statusHeight = ui->statusPanel->height();

    QSize size = ui->player->sizeHint();

    return QSize(size.width(), size.height() + frameHeight + statusHeight);
}

void VideoPlayer::mediaPositionChanged(qint64 pos) {
    emit positionChanged(pos);
}

void VideoPlayer::reload() {
    setFilename(mediaFilename);
}

#endif
