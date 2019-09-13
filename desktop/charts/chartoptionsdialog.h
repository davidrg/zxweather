#ifndef CHARTOPTIONSDIALOG_H
#define CHARTOPTIONSDIALOG_H

#include <QDialog>
#include <QDateTime>
#include <QList>

// AbstractLiveDatasource for hardware_type_t
#include "datasource/abstractlivedatasource.h"
#include "datasource/samplecolumns.h"

namespace Ui {
class ChartOptionsDialog;
}

class ChartOptionsDialog : public QDialog
{
    Q_OBJECT
    
public:

    explicit ChartOptionsDialog(bool solarAvailable, hardware_type_t hw_type, bool isWireless,
                                ExtraColumns extraColumns, QMap<ExtraColumn, QString> extraColumnNames,
                                QWidget *parent = 0);
    ~ChartOptionsDialog();
    
    QDateTime getStartTime();
    QDateTime getEndTime();
    SampleColumns getColumns();
    AggregateFunction getAggregateFunction();
    AggregateGroupType getAggregateGroupType();
    uint32_t getCustomMinutes();

private slots:
    void checkAndAccept();
    void checkboxToggled(bool checked);

private:
    Ui::ChartOptionsDialog *ui;
    SampleColumns columns;
    QMap<int, QString> tabLabels;
};

#endif // CHARTOPTIONSDIALOG_H
