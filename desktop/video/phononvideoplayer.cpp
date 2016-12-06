#include "phononvideoplayer.h"
#include "ui_phononvideoplayer.h"

#include <Phonon/MediaObject>
#include <QTime>
#include <QtDebug>

PhononVideoPlayer::PhononVideoPlayer(QWidget *parent) :
    AbstractVideoPlayer(parent),
    ui(new Ui::PhononVideoPlayer)
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

    Phonon::createPath(&mediaObject, ui->player);

    connect(&mediaObject, SIGNAL(finished()), this, SLOT(finished()));
    connect(&mediaObject, SIGNAL(totalTimeChanged(qint64)),
            this, SLOT(updateTime()));
    connect(&mediaObject, SIGNAL(tick(qint64)),
            this, SLOT(updateTime()));
    connect(&mediaObject, SIGNAL(stateChanged(Phonon::State,Phonon::State)),
            this, SLOT(stateChanged(Phonon::State)));

    mediaObject.setTickInterval(1000);

    setControlsEnabled(false);
    setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Expanding);
}

PhononVideoPlayer::~PhononVideoPlayer()
{
    delete ui;
}

void PhononVideoPlayer::play() {
    mediaObject.play();
    ui->tbPlay->setChecked(true);
}

void PhononVideoPlayer::pause() {
    mediaObject.pause();
    ui->tbPause->setChecked(true);
}

void PhononVideoPlayer::stop() {
    mediaObject.stop();
    ui->tbStop->setChecked(true);
}

void PhononVideoPlayer::setFilename(QString filename) {
    mediaObject.setCurrentSource(Phonon::MediaSource(filename));
    ui->lStatus->setText("Loading...");
}

void PhononVideoPlayer::finished() {
    ui->tbStop->setChecked(true);
}

void PhononVideoPlayer::updateTime() {
    ui->lTime->setText(timeString(mediaObject.totalTime(),
                                  mediaObject.currentTime()));
}

QSize PhononVideoPlayer::videoSize() {
    if (mediaObject.state() == Phonon::LoadingState) {
        return QSize();
    }
    return ui->player->sizeHint();
}

void PhononVideoPlayer::setControlsEnabled(bool enabled) {
    ui->tbPause->setEnabled(enabled);
    ui->tbPlay->setEnabled(enabled);
    ui->tbStop->setEnabled(enabled);
}

void PhononVideoPlayer::stateChanged(Phonon::State newState) {

    switch(newState) {
    case Phonon::LoadingState:
        ui->lStatus->setText("Loading...");
        setControlsEnabled(false);
        break;
    case Phonon::StoppedState:
        ui->lStatus->setText("Stopped");
        break;
    case Phonon::PlayingState:
        ui->lStatus->setText("Playing");
        break;
    case Phonon::BufferingState:
        ui->lStatus->setText("Buffering");
        break;
    case Phonon::PausedState:
        ui->lStatus->setText("Paused");
        break;
    case Phonon::ErrorState:
        ui->lStatus->setText("Error: " + mediaObject.errorString());
        setControlsEnabled(false);
        break;
    default:
        ui->lStatus->setText("");
    }

    if (newState != Phonon::LoadingState) {
        QSize size = videoSize();
        if (oldSize.width() != size.width() ||
                oldSize.height() != size.height()) {
            oldSize = size;
            qDebug() << "Size changed: " << size;
            emit sizeChanged(size);
            updateGeometry();
        }

        if (newState != Phonon::ErrorState) {
            setControlsEnabled(true);
        }
    }
}

QSize PhononVideoPlayer::sizeHint() const {
    int frameHeight = ui->frame->height();
    int statusHeight = ui->statusPanel->height();

    QSize size = ui->player->sizeHint();

    return QSize(size.width(), size.height() + frameHeight + statusHeight);
}
