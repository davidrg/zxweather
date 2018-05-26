#ifndef EXPORTDIALOG_H
#define EXPORTDIALOG_H

#include <QDialog>
#include <QDateTime>

#include "datasource/abstractdatasource.h"

namespace Ui {
class ExportDialog;
}

class ExportDialog : public QDialog
{
    Q_OBJECT
    
public:
    explicit ExportDialog(bool solarDataAvailable, hardware_type_t hw_type, QWidget *parent = 0);
    ~ExportDialog();

private slots:
    void exportData();
    void delimiterTypeChanged();
    void samplesReady(SampleSet samples);
    void samplesFailed(QString message);
    
private:
    Ui::ExportDialog *ui;
    QScopedPointer<AbstractDataSource> dataSource;

    QString targetFilename;
    bool dashNulls;

    QString getDelimiter();
    QDateTime getStartTime();
    QDateTime getEndTime();
    SampleColumns getColumns();
    QString getHeaderRow(SampleColumns columns);
    QString dstr(double v, bool nodp=false);
};

#endif // EXPORTDIALOG_H
