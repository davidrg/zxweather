#include "videoplayer.h"
#include "ui_videoplayer.h"

#include <QTime>

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

    mediaObject.setVideoOutput(ui->player);

    setTickInterval(1000);

    connect(&mediaObject, SIGNAL(durationChanged(qint64)),
            this, SLOT(updateTime()));
    connect(&mediaObject, SIGNAL(positionChanged(qint64)),
            this, SLOT(updateTime()));
    connect(&mediaObject, SIGNAL(mediaStatusChanged(QMediaPlayer::MediaStatus)),
            this, SLOT(mediaStatusChanged(QMediaPlayer::MediaStatus)));
    connect(&mediaObject, SIGNAL(stateChanged(QMediaPlayer::State)),
            this, SLOT(stateChanged(QMediaPlayer::State)));
    connect(&mediaObject, SIGNAL(error(QMediaPlayer::Error)),
            this, SLOT(mediaError(QMediaPlayer::Error)));
    connect(&mediaObject, SIGNAL(positionChanged(qint64)),
            this, SLOT(mediaPositionChanged(qint64)));

    setControlsEnabled(false);
}

VideoPlayer::~VideoPlayer()
{
    delete ui;
}

void VideoPlayer::setTickInterval(qint32 interval) {
    mediaObject.setNotifyInterval(interval);
}

void VideoPlayer::play() {
    mediaObject.play();
    ui->tbPlay->setChecked(true);
}

void VideoPlayer::pause() {
    mediaObject.pause();
    ui->tbPause->setChecked(true);
}

void VideoPlayer::stop() {
    mediaObject.stop();
    ui->tbStop->setChecked(true);
}

void VideoPlayer::setFilename(QString filename) {
    mediaObject.setMedia(QUrl::fromLocalFile(filename));
    setStatus("Loading...");
}

void VideoPlayer::finished() {
    ui->tbStop->setChecked(true);
}

void VideoPlayer::updateTime() {
    ui->lTime->setText(timeString(mediaObject.duration(),
                                  mediaObject.position()));
}

bool VideoPlayer::controlsEnabled() const {
    return ui->tbPause->isEnabled();
}

void VideoPlayer::setControlsEnabled(bool enabled) {
    if (mediaObject.mediaStatus() != QMediaPlayer::LoadedMedia) {
        return; // Not valid to enable controls yet.
    }

    ui->tbPause->setEnabled(enabled);
    ui->tbPlay->setEnabled(enabled);
    ui->tbStop->setEnabled(enabled);
}

void VideoPlayer::setStatus(QString status) {
    ui->lStatus->setText(status);
}

QSize VideoPlayer::videoSize() {
    if (mediaObject.mediaStatus() == QMediaPlayer::LoadingMedia) {
        return QSize();
    }
    return ui->player->sizeHint();
}

void VideoPlayer::mediaStatusChanged(QMediaPlayer::MediaStatus newStatus) {
    switch (newStatus) {
    case QMediaPlayer::LoadingMedia:
        setStatus("Loading...");
        setControlsEnabled(false);
        break;
    case QMediaPlayer::EndOfMedia:
        setStatus("Paused");
        ui->tbPause->setChecked(true);
        break;
    case QMediaPlayer::LoadedMedia:
        setStatus("Stopped");
        if (!controlsLocked) {
            setControlsEnabled(true);
        }
        emit ready();
        break;
    case QMediaPlayer::BufferingMedia:
//        setStatus("Buffering");
        break;
    case QMediaPlayer::StalledMedia:
//        setStatus("Stalled");
        break;
    case QMediaPlayer::BufferedMedia:
//        setStatus("Buffered");
        break;
    case QMediaPlayer::InvalidMedia:
        setStatus("Invalid Media");
        setControlsEnabled(false);
        break;
    case QMediaPlayer::UnknownMediaStatus:
    case QMediaPlayer::NoMedia:

    default:
        setStatus("No Media");
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
    switch (newState) {
    // We handle this via media status changed so we can handle stopped vs end of media
//    case QMediaPlayer::StoppedState:
//        setStatus("Stopped");
//        break;
    case QMediaPlayer::PlayingState:
        setStatus("Playing");
        break;
    case QMediaPlayer::PausedState:
        setStatus("Paused");
        break;
    }
}

void VideoPlayer::mediaError(QMediaPlayer::Error /*error*/) {
    setStatus("Error: " + mediaObject.errorString());
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
