#ifndef LIVEDATAWIDGET_H
#define LIVEDATAWIDGET_H

#include <QWidget>
#include <QIcon>
#include <QHash>
#include "datasource/abstractlivedatasource.h"

namespace Ui {
class LiveDataWidget;
}

class LiveDataWidget : public QWidget
{
    Q_OBJECT
    
public:
    explicit LiveDataWidget(QWidget *parent = 0);
    ~LiveDataWidget();
    
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
    Ui::LiveDataWidget *ui;

    void createDatabaseDataSource();
    void createJsonDataSource();

    void refreshUi(LiveDataSet lds);
    void refreshSysTrayText(LiveDataSet lds);
    void refreshSysTrayIcon(LiveDataSet lds);

    QString previousSysTrayText;
    QString previousSysTrayIcon;

    QScopedPointer<AbstractLiveDataSource> dataSource;

    uint seconds_since_last_refresh;
    uint minutes_late;

    uint updateCount;

    QTimer* ldTimer;

    QHash<int,QString> forecastRules;

    void loadForecastRules();

};

#endif // LIVEDATAWIDGET2_H
