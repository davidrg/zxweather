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
    explicit ExportDialog(QWidget *parent = 0);
    ~ExportDialog();

private slots:
    void exportData();
    void dateChanged();
    void delimiterTypeChanged();
    void samplesReady(SampleSet samples);
    void samplesFailed(QString message);
    
private:
    typedef enum {
        C_TIMESTAMP,
        C_TEMPERATURE,
        C_INDOOR_TEMPERATURE,
        C_HUMIDITY,
        C_INDOOR_HUMIDITY,
        C_APPARENT_TEMPERATURE,
        C_WIND_CHILL,
        C_DEW_POINT,
        C_PRESSURE,
        C_RAINFALL,
        C_AVG_WIND_SPEED,
        C_GUST_WIND_SPEED,
        C_WIND_DIRECTION
    } COLUMNS;

    Ui::ExportDialog *ui;
    QScopedPointer<AbstractDataSource> dataSource;

    QString targetFilename;

    QString getDelimiter();
    QDateTime getStartTime();
    QDateTime getEndTime();
    QSet<COLUMNS> getColumns();
    QList<COLUMNS> columnList(QSet<COLUMNS> columns);
    QString getHeaderRow(QList<COLUMNS> columns);
};

#endif // EXPORTDIALOG_H
