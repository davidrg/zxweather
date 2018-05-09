#ifndef RUNREPORTDIALOG_H
#define RUNREPORTDIALOG_H

#include <QDialog>
#include <QDate>
#include <QDateTime>

#include "report.h"
#include "datasource/abstractdatasource.h"

namespace Ui {
class RunReportDialog;
}

class QTreeWidgetItem;

class RunReportDialog : public QDialog
{
    Q_OBJECT

public:
    explicit RunReportDialog(QWidget *parent = 0);
    ~RunReportDialog();

    typedef enum {
        Page_None = -1,
        Page_ReportSelect = 0,
        Page_Timespan = 1,
        Page_Criteria = 2,
        Page_Finish = 99
    } Page;

private slots:
    void reportSelected(QTreeWidgetItem* twi, QTreeWidgetItem *prev);

    void moveNextPage();
    void movePreviousPage();
    void cancel();

    void timespanSelected();

private:
    Ui::RunReportDialog *ui;
    Report report;
    Page nextPage;
    Page previousPage;

    QScopedPointer<AbstractDataSource> ds;

    typedef struct {
        QDate start;
        QDate end;
    } date_span_t;

    typedef struct {
        QDateTime start;
        QDateTime end;
    } time_span_t;

    QDate get_date();
    QDate get_month();
    int get_year();
    date_span_t get_date_span();
    time_span_t get_time_span();

    void switchPage(Page page);
    void runReport();

    bool needsCriteriaPageCreated;
    void createReportCriteria();
    void loadReportCriteria();
};

#endif // RUNREPORTDIALOG_H
