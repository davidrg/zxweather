#ifndef LIVEDATAWIDGET_H
#define LIVEDATAWIDGET_H

#include <QWidget>
#include <QIcon>
#include "livedatasource.h"

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
    void liveDataRefreshed();

    /** For monitoring live data. This (and the associated time ldTimer) is
     * what pops up warnings when live data is late.
     */
    void liveTimeout();

    // Database errors (for use with the database data source)

    /**
     * @brief Called when connecting to the database fails.
     *
     * It displays a popup message from the system tray icon containing details
     * of the problem.
     */
    void connection_failed(QString);

    /**
     * @brief Called whenever a database error occurs that is not a connection
     * failure.
     *
     * @param message The error message from the database layer.
     */
    void unknown_db_error(QString message);

    /** An error from the Json Data Source.
     *
     * @param message The error message.
     */
    void networkError(QString message);

private:
    void createDatabaseDataSource();
    void createJsonDataSource();

    void refreshUi(AbstractLiveData* data);
    void refreshSysTrayText(AbstractLiveData *data);
    void refreshSysTrayIcon(AbstractLiveData *data);

    QLabel* lblRelativeHumidity;
    QLabel* lblTemperature;
    QLabel* lblDewPoint;
    QLabel* lblWindChill;
    QLabel* lblApparentTemperature;
    QLabel* lblAbsolutePressure;
    QLabel* lblAverageWindSpeed;
    QLabel* lblGustWindSpeed;
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
    QLabel* lblLabelGustWindSpeed;
    QLabel* lblLabelWindDirection;
    QLabel* lblLabelTimestamp;

    QGridLayout* gridLayout;
    QFrame* line;

    QString previousSysTrayText;
    QString previousSysTrayIcon;

    QScopedPointer<AbstractLiveDataSource> dataSource;

    uint seconds_since_last_refresh;
    uint minutes_late;

    QTimer* ldTimer;
};

#endif // LIVEDATAWIDGET_H
