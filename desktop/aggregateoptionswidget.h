#ifndef AGGREGATEOPTIONSWIDGET_H
#define AGGREGATEOPTIONSWIDGET_H

#include <QWidget>

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

private:
    Ui::AggregateOptionsWidget *ui;
};

#endif // AGGREGATEOPTIONSWIDGET_H
