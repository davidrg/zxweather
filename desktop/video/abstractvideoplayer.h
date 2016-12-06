#ifndef ABSTRACTVIDEOPLAYER_H
#define ABSTRACTVIDEOPLAYER_H

#include <QWidget>

/** A simple video playback widget with play/pause/stop controls plus
 * duration and current time indicators.
 */
class AbstractVideoPlayer : public QWidget
{
    Q_OBJECT
public:
    explicit AbstractVideoPlayer(QWidget *parent = 0);

    virtual void setFilename(QString filename) = 0;

    /** Constructs an AbstractVideoPlayer subclass appropriate for the current
     * Qt version in use. This could be backed by either Phonon (for Qt 4.8)
     * or QtMultimedia (for Qt 5.x)
     *
     * @param parent Parent widget for the new video player widget.
     * @return A new AbstractVideoPlayer instance
     */
    static AbstractVideoPlayer * createVideoPlayer(QWidget *parent);

    virtual QSize videoSize() = 0;
signals:
    void sizeChanged(QSize size);

public slots:
    virtual void play() = 0;
    virtual void pause() = 0;
    virtual void stop() = 0;

protected:
    QString timeString(qint64 length, qint64 position) const;
};

#endif // ABSTRACTVIDEOPLAYER_H
