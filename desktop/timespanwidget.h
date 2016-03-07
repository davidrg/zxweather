#ifndef TIMESPANWIDGET_H
#define TIMESPANWIDGET_H

#include <QWidget>
#include <QDateTime>

namespace Ui {
class TimespanWidget;
}

class TimespanWidget : public QWidget
{
    Q_OBJECT

public:
    explicit TimespanWidget(QWidget *parent = 0);
    ~TimespanWidget();

    QDateTime getStartTime();
    QDateTime getEndTime();

    void setTimeSpan(QDateTime start, QDateTime end);

private slots:
    void dateChanged();

private:
    Ui::TimespanWidget *ui;
};

#endif // TIMESPANWIDGET_H
