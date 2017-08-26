#ifndef WEATHERIMAGEWINDOW_H
#define WEATHERIMAGEWINDOW_H

#include <QWidget>
#include <QScopedPointer>
#include <QStringList>
#include "datasource/abstractdatasource.h"

namespace Ui {
class WeatherImageWindow;
}

class WeatherImageWindow : public QWidget
{
    Q_OBJECT

public:
    explicit WeatherImageWindow(QWidget *parent = 0);
    ~WeatherImageWindow();

    void setImage(int imageId);

private slots:
    void imageReady(ImageInfo imageInfo, QImage image, QString filename);
    void samplesReady(SampleSet samples);
    void mediaPositionChanged(qint64 time);

private:
    Ui::WeatherImageWindow *ui;
    QScopedPointer<AbstractDataSource> dataSource;
    bool isImage;
    QStringList windDirections;
    double rainTotal;

    void displaySample(SampleSet samples, int i);

    // For video playback:
    bool videoSync;
    QDateTime videoStart, videoEnd;
    uint frameInterval, frameCount, framesPerSecond;
    SampleSet videoSamples;
    int msPerSample;
};

#endif // WEATHERIMAGEWINDOW_H
