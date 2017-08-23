#ifndef PHONONVIDEOPLAYER_H
#define PHONONVIDEOPLAYER_H

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

public slots:
    void play();
    void pause();
    void stop();

protected:
    void resizeEvent(QResizeEvent * event);

private slots:
    void finished();
    void updateTime();
    void stateChanged(Phonon::State newState);
    void setControlsEnabled(bool enabled);

private:
    Ui::PhononVideoPlayer *ui;
    Phonon::MediaObject mediaObject;
    QSize oldSize;
    bool resized;
};

#endif // PHONONVIDEOPLAYER_H
