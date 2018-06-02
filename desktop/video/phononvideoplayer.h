#ifndef PHONONVIDEOPLAYER_H
#define PHONONVIDEOPLAYER_H

#ifndef NO_MULTIMEDIA

#include <QWidget>
#include <Phonon/MediaObject>

#include "abstractvideoplayer.h"

namespace Ui {
class PhononVideoPlayer;
}

/** Video Player for Qt 4.8 using Phonon
 *
 */
class PhononVideoPlayer : public AbstractVideoPlayer
{
    Q_OBJECT

public:
    explicit PhononVideoPlayer(QWidget *parent = 0);
    ~PhononVideoPlayer();

    void setFilename(QString filename);

    QSize videoSize();

    QSize sizeHint() const;

    bool controlsEnabled() const;

public slots:
    void play();
    void pause();
    void stop();
    void mediaTick(qint64 time);
    void setTickInterval(qint32 interval);
    void setControlsEnabled(bool enabled);

protected:
    void resizeEvent(QResizeEvent * event);

private slots:
    void finished();
    void updateTime();
    void stateChanged(Phonon::State newState);

private:
    Ui::PhononVideoPlayer *ui;
    Phonon::MediaObject mediaObject;
    QSize oldSize;
    bool resized;
    bool controlsLocked;
};

#endif // NO_MULTIMEDIA
#endif // PHONONVIDEOPLAYER_H
