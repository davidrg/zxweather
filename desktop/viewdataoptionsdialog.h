#ifndef VIEWDATAOPTIONSDIALOG_H
#define VIEWDATAOPTIONSDIALOG_H

#include <QDialog>
#include <QDateTime>
#include <stdint.h>

#include "datasource/aggregate.h"
#include "datasource/samplecolumns.h"
#include "datasource/abstractdatasource.h"


namespace Ui {
class ViewDataOptionsDialog;
}

class ViewDataOptionsDialog : public QDialog
{
    Q_OBJECT

public:
    explicit ViewDataOptionsDialog(bool solarAvailable, hardware_type_t hw_type, bool isWireless,
                                   ExtraColumns extraColumns, QMap<ExtraColumn, QString> extraColumnNames, QWidget *parent = 0);
    ~ViewDataOptionsDialog();

    QDateTime getStartTime() const;
    QDateTime getEndTime() const;
    AggregateFunction getAggregateFunction() const;
    AggregateGroupType getAggregateGroupType() const;
    uint32_t getCustomMinutes() const;
    SampleColumns getColumns() const;

private:
    Ui::ViewDataOptionsDialog *ui;
};

#endif // VIEWDATAOPTIONSDIALOG_H
