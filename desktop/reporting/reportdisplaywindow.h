#ifndef REPORTDISPLAYWINDOW_H
#define REPORTDISPLAYWINDOW_H

#include <QDialog>
#include <QAbstractTableModel>

#include "report.h"

class QTabWidget;

class ReportDisplayWindow : public QDialog
{
    Q_OBJECT
public:
    explicit ReportDisplayWindow(QString reportName, QIcon reportIcon, QWidget *parent = NULL);

    void addHtmlTab(QString name, QIcon icon, QString content);
    void addPlainTab(QString name, QIcon icon, QString text);
    void addGridTab(QString name, QIcon icon, QAbstractTableModel *model, QStringList hideColumns);

    void setSaveOutputs(QList<report_output_file_t> outputs);

private slots:
    void copyGridSelection();
    void saveReport();

private:
    QTabWidget *tabs;
    QList<report_output_file_t> outputs;
};

#endif // REPORTDISPLAYWINDOW_H
