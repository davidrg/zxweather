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
    explicit ExportDialog(bool solarDataAvailable, QWidget *parent = 0);
    ~ExportDialog();

private slots:
    void exportData();
    void dateChanged();
    void delimiterTypeChanged();
    void samplesReady(SampleSet samples);
    void samplesFailed(QString message);
    
private:
    Ui::ExportDialog *ui;
    QScopedPointer<AbstractDataSource> dataSource;

    QString targetFilename;

    QString getDelimiter();
    QDateTime getStartTime();
    QDateTime getEndTime();
    SampleColumns getColumns();
    QString getHeaderRow(SampleColumns columns);
};

#endif // EXPORTDIALOG_H
