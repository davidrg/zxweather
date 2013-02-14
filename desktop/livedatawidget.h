#ifndef LIVEDATAWIDGET_H
#define LIVEDATAWIDGET_H

#include <QWidget>
#include <QIcon>
#include <QHBoxLayout>
#include <QHash>
#include "datasource/abstractlivedatasource.h"

class QLabel;
class QGridLayout;
class QFrame;
class QTimer;

class LiveDataWidget : public QWidget
{
    Q_OBJECT
public:
    explicit LiveDataWidget(QWidget *parent = 0);
    
    /** Reconnects to the datasource. Call this when ever data source
     * settings are changed.
     */
    void reconfigureDataSource();
signals:
    void sysTrayTextChanged(QString text);
    void sysTrayIconChanged(QIcon icon);

    void warning(
            QString message,
            QString title,
            QString tooltip,
            bool setWarningIcon);

private slots:
    /** Called when new live data is available.
     *
     * @param data The new live data.
     */
    void liveDataRefreshed(LiveDataSet lds);

    /** For monitoring live data. This (and the associated time ldTimer) is
     * what pops up warnings when live data is late.
     */
    void liveTimeout();

    // Database errors (for use with the database data source)

    /**
     * @brief Called when something goes wrong.
     */
    void error(QString);

private:
    void createDatabaseDataSource();
    void createJsonDataSource();

    void refreshUi(LiveDataSet lds);
    void refreshSysTrayText(LiveDataSet lds);
    void refreshSysTrayIcon(LiveDataSet lds);

    QLabel* lblRelativeHumidity;
    QLabel* lblTemperature;
    QLabel* lblDewPoint;
    QLabel* lblWindChill;
    QLabel* lblApparentTemperature;
    QLabel* lblAbsolutePressure;
    QLabel* lblAverageWindSpeed;
    QLabel* lblWindDirection;
    QLabel* lblTimestamp;

    QLabel* lblTitle;
    QLabel* lblLabelRelativeHumidity;
    QLabel* lblLabelTemperature;
    QLabel* lblLabelDewPoint;
    QLabel* lblLabelWindChill;
    QLabel* lblLabelApparentTemperature;
    QLabel* lblLabelAbsolutePressure;
    QLabel* lblLabelAverageWindSpeed;
    QLabel* lblLabelWindDirection;
    QLabel* lblLabelTimestamp;

    // Davis stuff
    QLabel* lblLabelRainRate;
    QLabel* lblLabelStormRain;
    QLabel* lblLabelCurrentStormStartDate;
    QLabel* lblLabelBarometerTrend;
    QLabel* lblLabelTxBattery;
    QLabel* lblLabelConsoleBattery;
    QHBoxLayout* forecastLayout;

    QLabel* lblRainRate;
    QLabel* lblStormRain;
    QLabel* lblCurrentStormStartDate;
    QLabel* lblBarometerTrend;
    QLabel* lblTxBattery;
    QLabel* lblConsoleBattery;

    QGridLayout* gridLayout;
    QFrame* line;

    QLabel* lblForecastTitle;
    QFrame* forecastLine;
    QLabel* lblForecast;

    QString previousSysTrayText;
    QString previousSysTrayIcon;

    QScopedPointer<AbstractLiveDataSource> dataSource;

    uint seconds_since_last_refresh;
    uint minutes_late;

    QTimer* ldTimer;

    QHash<int,QString> forecastRules;

    void loadForecastRules();
};

#endif // LIVEDATAWIDGET_H
