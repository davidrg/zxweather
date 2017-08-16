#ifndef FORECASTWIDGET_H
#define FORECASTWIDGET_H

#include <QWidget>
#include <QHash>

#include "datasource/abstractlivedatasource.h"

namespace Ui {
class ForecastWidget;
}

class ForecastWidget : public QWidget
{
    Q_OBJECT
    
public:
    explicit ForecastWidget(QWidget *parent = 0);
    ~ForecastWidget();
    
public slots:
    /** Called when new live data is available.
     *
     * @param data The new live data.
     */
    void refreshLiveData(LiveDataSet lds);

private:
    Ui::ForecastWidget *ui;
    QHash<int,QString> forecastRules;

    void loadForecastRules();
};

#endif // FORECASTWIDGET_H
