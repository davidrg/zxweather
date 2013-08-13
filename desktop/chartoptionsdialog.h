#ifndef CHARTOPTIONSDIALOG_H
#define CHARTOPTIONSDIALOG_H

#include <QDialog>
#include <QDateTime>
#include <QList>

#define COL_TEMPERATURE 1
#define COL_TEMPERATURE_INDOORS 2
#define COL_APPARENT_TEMPERATURE 3
#define COL_WIND_CHILL 4
#define COL_DEW_POINT 5
#define COL_HUMIDITY 6
#define COL_HUMIDITY_INDOORS 7
#define COL_PRESSURE 8
#define COL_RAINFALL 9
#define COL_AVG_WINDSPEED 10
#define COL_GUST_WINDSPEED 11
#define COL_WIND_DIRECTION 12

namespace Ui {
class ChartOptionsDialog;
}

class ChartOptionsDialog : public QDialog
{
    Q_OBJECT
    
public:

    explicit ChartOptionsDialog(QWidget *parent = 0);
    ~ChartOptionsDialog();
    
    QDateTime getStartTime();
    QDateTime getEndTime();
    QList<int> getColumns();

private slots:
    void dateChanged();
    void checkAndAccept();

private:
    Ui::ChartOptionsDialog *ui;
    QList<int> columns;
};

#endif // CHARTOPTIONSDIALOG_H
