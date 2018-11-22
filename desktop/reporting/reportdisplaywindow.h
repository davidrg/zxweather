#ifndef REPORTDISPLAYWINDOW_H
#define REPORTDISPLAYWINDOW_H

#include <QDialog>
#include <QAbstractTableModel>
#include <QUrl>

#include "report.h"
#include "datasource/samplecolumns.h"

class QTabWidget;

class ReportDisplayWindow : public QDialog
{
    Q_OBJECT
public:
    explicit ReportDisplayWindow(QString reportName, QIcon reportIcon, QWidget *parent = NULL);

    void addHtmlTab(QString name, QIcon icon, QString content);
    void addPlainTab(QString name, QIcon icon, QString text, bool wordWrap);
    void addGridTab(QString name, QIcon icon, QAbstractTableModel *model, QStringList hideColumns);

    void setSaveOutputs(QList<report_output_file_t> outputs);

    void setStationInfo(bool hasSolarData, bool isWireless) {
        solarDataAvailable = hasSolarData; wirelessAvailable = isWireless;
    }

private slots:
    void copyGridSelection();
    void saveReport();
    void linkClicked(QUrl url);

private:
    QTabWidget *tabs;
    QList<report_output_file_t> outputs;
    bool solarDataAvailable;
    bool wirelessAvailable;
};

#endif // REPORTDISPLAYWINDOW_H
