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

    virtual bool controlsEnabled() const = 0;

signals:
    void sizeChanged(QSize size);
    void positionChanged(qint64 time); /*!< emitted as the video plays to indicate time */
    void ready(); /*!< Emitted when ready for playback */

public slots:
    virtual void play() = 0;
    virtual void pause() = 0;
    virtual void stop() = 0;
    virtual void setTickInterval(qint32 interval) = 0;
    virtual void setControlsEnabled(bool enabled) = 0;
    virtual void setControlsLocked(bool locked);

protected:
    QString timeString(qint64 length, qint64 position) const;

    bool controlsLocked;
};

#endif // ABSTRACTVIDEOPLAYER_H
