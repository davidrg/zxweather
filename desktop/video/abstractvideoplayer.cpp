#include "abstractvideoplayer.h"

#include <QTime>

#if QT_VERSION >= 0x050000
    #include "videoplayer.h"
#else
    #include "phononvideoplayer.h"
#endif

AbstractVideoPlayer::AbstractVideoPlayer(QWidget *parent) : QWidget(parent)
{
}

AbstractVideoPlayer* AbstractVideoPlayer::createVideoPlayer(QWidget *parent) {
#if QT_VERSION >= 0x050000
    return new VideoPlayer(parent);
#else
    return new PhononVideoPlayer(parent);
#endif
}

QString AbstractVideoPlayer::timeString(qint64 length, qint64 position) const {
    QString timeString;
    if (position > 0 || length > 0)
    {
        int sec = position/1000;
        int min = sec/60;
        int hour = min/60;
        int msec = position;

        QTime playTime(hour%60, min%60, sec%60, msec%1000);
        sec = length / 1000;
        min = sec / 60;
        hour = min / 60;
        msec = length;

        QTime stopTime(hour%60, min%60, sec%60, msec%1000);
        QString timeFormat = "m:ss";
        if (hour > 0)
            timeFormat = "h:mm:ss";
        timeString = playTime.toString(timeFormat);
        if (length)
            timeString += " / " + stopTime.toString(timeFormat);
    }

    return timeString;
}
