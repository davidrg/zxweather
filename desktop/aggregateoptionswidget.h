#ifndef AGGREGATEOPTIONSWIDGET_H
#define AGGREGATEOPTIONSWIDGET_H

#include <QWidget>
#include <stdint.h>

#include "datasource/aggregate.h"

namespace Ui {
class AggregateOptionsWidget;
}

class AggregateOptionsWidget : public QWidget
{
    Q_OBJECT

public:
    explicit AggregateOptionsWidget(QWidget *parent = 0);
    ~AggregateOptionsWidget();

    AggregateFunction getAggregateFunction();
    AggregateGroupType getAggregateGroupType();
    uint32_t getCustomMinutes();

    void setRainEvapoOptionsEnabled(bool enabled);
    bool isRainEvapoOptionsEnabled();

private:
    Ui::AggregateOptionsWidget *ui;
    bool rainEvapoOptionsEnabled;
};

#endif // AGGREGATEOPTIONSWIDGET_H
