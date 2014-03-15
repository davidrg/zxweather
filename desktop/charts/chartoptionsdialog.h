#ifndef CHARTOPTIONSDIALOG_H
#define CHARTOPTIONSDIALOG_H

#include <QDialog>
#include <QDateTime>
#include <QList>

#include "datasource/samplecolumns.h"

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
    SampleColumns getColumns();

private slots:
    void dateChanged();
    void checkAndAccept();

private:
    Ui::ChartOptionsDialog *ui;
    SampleColumns columns;
};

#endif // CHARTOPTIONSDIALOG_H