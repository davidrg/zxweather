#ifndef VIEWDATAOPTIONSDIALOG_H
#define VIEWDATAOPTIONSDIALOG_H

#include <QDialog>
#include <QDateTime>
#include <stdint.h>

#include "datasource/aggregate.h"

namespace Ui {
class ViewDataOptionsDialog;
}

class ViewDataOptionsDialog : public QDialog
{
    Q_OBJECT

public:
    explicit ViewDataOptionsDialog(QWidget *parent = 0);
    ~ViewDataOptionsDialog();

    QDateTime getStartTime();
    QDateTime getEndTime();
    AggregateFunction getAggregateFunction();
    AggregateGroupType getAggregateGroupType();
    uint32_t getCustomMinutes();

private:
    Ui::ViewDataOptionsDialog *ui;
};

#endif // VIEWDATAOPTIONSDIALOG_H
