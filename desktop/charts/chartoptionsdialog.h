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
    
    QDateTime getStartTime() const;
    QDateTime getEndTime() const;
    SampleColumns getColumns() const;
    AggregateFunction getAggregateFunction() const;
    AggregateGroupType getAggregateGroupType() const;
    uint32_t getCustomMinutes() const;

private slots:
    void checkAndAccept();

private:
    Ui::ChartOptionsDialog *ui;
};

#endif // CHARTOPTIONSDIALOG_H
