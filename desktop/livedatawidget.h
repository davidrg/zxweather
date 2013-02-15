#ifndef LIVEDATAWIDGET_H
#define LIVEDATAWIDGET_H

#include <QWidget>
#include <QIcon>

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
    
public slots:
    /** Called when new live data is available.
     *
     * @param data The new live data.
     */
    void refreshLiveData(LiveDataSet lds);

signals:
    void sysTrayTextChanged(QString text);
    void sysTrayIconChanged(QIcon icon);

private:
    Ui::LiveDataWidget *ui;

    void reconfigureUI(hardware_type_t hw_type);

    void refreshUi(LiveDataSet lds);
    void refreshSysTrayText(LiveDataSet lds);
    void refreshSysTrayIcon(LiveDataSet lds);

    QString previousSysTrayText;
    QString previousSysTrayIcon;
};

#endif // LIVEDATAWIDGET2_H
