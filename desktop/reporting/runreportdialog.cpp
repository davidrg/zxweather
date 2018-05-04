#include "runreportdialog.h"
#include "ui_runreportdialog.h"

#include "report.h"
#include "settings.h"
#include "datasource/databasedatasource.h"
#include "datasource/webdatasource.h"
#include "datasource/dialogprogresslistener.h"

#include <QDebug>
#include <QTreeWidgetItem>

/* TODO:
 *  - Intro page for when no report is selected
 *  - Double clicking a report in the list should proceed to the next page
 *  - Filter out reports that don't support the current hardware type
 *  - Add a button to the main window instead of displaying this window on startup
 *  - Custom report criteria UI
 *  - Update copyright info in about dialog (Qt Mustache)
 *
 * Report class TODO:
 *  - Load supported weather station
 *  - Load custom criteria UI
 *  - Output to disk
 *  - Output window
 *      - Save buttons - CSV for grid, html or plain for other
 *      - CSV copy&paste for grid
 *
 * DB Data Source TODO:
 *  - Change column names to match the Postgres schema
 *  - Make station code available in the DB. Should be full URL with code supplied to
 *    query as also the full url.
 *  - make hw-type and other station config details available
 */

RunReportDialog::RunReportDialog(QWidget *parent) :
    QDialog(parent),
    ui(new Ui::RunReportDialog)
{
    ui->setupUi(this);

    QList<Report> reports = Report::loadReports();

    bool isWebDs = Settings::getInstance().sampleDataSourceType() == Settings::DS_TYPE_WEB_INTERFACE;
    bool isDbDs = Settings::getInstance().sampleDataSourceType() == Settings::DS_TYPE_DATABASE;

    foreach (Report r, reports) {
        if (r.isNull())
            continue;

        if (isWebDs && !r.supportsWebDS()) {
            continue; // Report not compatible with the current data source
        }

        if (isDbDs && !r.supportsDBDS()) {
            continue; // Report not compatible with the current data source
        }

        QTreeWidgetItem *twi = new QTreeWidgetItem();
        twi->setText(0, r.title());
        twi->setData(0, Qt::UserRole, r.name());
        twi->setIcon(0, r.icon());
        ui->treeWidget->addTopLevelItem(twi);
    }

    connect(ui->treeWidget, SIGNAL(currentItemChanged(QTreeWidgetItem*,QTreeWidgetItem*)),
            this, SLOT(reportSelected(QTreeWidgetItem*,QTreeWidgetItem*)));
    connect(ui->pbCancel, SIGNAL(clicked(bool)), this, SLOT(cancel()));
    connect(ui->pbNext, SIGNAL(clicked(bool)), this, SLOT(moveNextPage()));
    connect(ui->pbBack, SIGNAL(clicked(bool)), this, SLOT(movePreviousPage()));

    connect(ui->rbDate, SIGNAL(toggled(bool)), this, SLOT(timespanSelected()));
    connect(ui->rbDateSpan, SIGNAL(toggled(bool)), this, SLOT(timespanSelected()));
    connect(ui->rbLastMonth, SIGNAL(toggled(bool)), this, SLOT(timespanSelected()));
    connect(ui->rbLastWeek, SIGNAL(toggled(bool)), this, SLOT(timespanSelected()));
    connect(ui->rbLastYear, SIGNAL(toggled(bool)), this, SLOT(timespanSelected()));
    connect(ui->rbMonth, SIGNAL(toggled(bool)), this, SLOT(timespanSelected()));
    connect(ui->rbThisMonth, SIGNAL(toggled(bool)), this, SLOT(timespanSelected()));
    connect(ui->rbThisWeek, SIGNAL(toggled(bool)), this, SLOT(timespanSelected()));
    connect(ui->rbThisYear, SIGNAL(toggled(bool)), this, SLOT(timespanSelected()));
    connect(ui->rbTimeSpan, SIGNAL(toggled(bool)), this, SLOT(timespanSelected()));
    connect(ui->rbToday, SIGNAL(toggled(bool)), this, SLOT(timespanSelected()));
    connect(ui->rbYear, SIGNAL(toggled(bool)), this, SLOT(timespanSelected()));
    connect(ui->rbYesterday, SIGNAL(toggled(bool)), this, SLOT(timespanSelected()));

    ui->pbNext->setEnabled(false);
    ui->pbBack->setEnabled(false);

    previousPage = Page_None;
    nextPage = Page_None;

    QDate d = QDate::currentDate();
    ui->deEndDate->setDate(d);
    ui->deStartDate->setDate(d.addDays(-7));
    ui->deDate->setDate(d.addDays(-2));
    d.setDate(d.year(), d.month(), 1);
    ui->deMonth->setDate(d.addMonths(-2));
    d.setDate(d.year(), 1, 1);
    ui->deYear->setDate(d.addYears(-2));

    QDateTime t = QDateTime::currentDateTime();
    ui->teEndTime->setDateTime(t);
    ui->teStartTime->setDateTime(t.addDays(-7));

    ui->splitter->setStretchFactor(0, 1);
    ui->splitter->setStretchFactor(1, 2);
}

RunReportDialog::~RunReportDialog()
{
    delete ui;
}

void RunReportDialog::reportSelected(QTreeWidgetItem* twi, QTreeWidgetItem *prev) {
    Q_UNUSED(prev);

    qDebug() << "Report Selected!";

    QString reportName = twi->data(0, Qt::UserRole).toString();
    report = Report(reportName);
    ui->textBrowser->setHtml(report.description());
    ui->lblReportTitle->setText("<h1>" + report.title() + "</h1>");
    switchPage(Page_ReportSelect);
    ui->pbNext->setEnabled(true);

    ui->rbDate->setEnabled(true);
    ui->rbDateSpan->setEnabled(true);
    ui->rbLastMonth->setEnabled(true);
    ui->rbLastWeek->setEnabled(true);
    ui->rbLastYear->setEnabled(true);
    ui->rbMonth->setEnabled(true);
    ui->rbThisMonth->setEnabled(true);
    ui->rbThisWeek->setEnabled(true);
    ui->rbThisYear->setEnabled(true);
    ui->rbTimeSpan->setEnabled(true);
    ui->rbToday->setEnabled(true);
    ui->rbYear->setEnabled(true);
    ui->rbYesterday->setEnabled(true);

    ui->pbNext->setText(tr("&Next >"));

    // The differet time parameters are inherited. Allowing a timespan implies
    // allowing a datespan (morining on start date to evening on end date) or a
    // single date (morning to evening) or a single month (start of month to end), etc.
    switch(report.timePickerType()) {
    case Report::TP_None:
    default:
        // no years
        ui->rbThisYear->setEnabled(false);
        ui->rbLastYear->setEnabled(false);
        ui->rbYear->setEnabled(false);

        // no months
        ui->rbThisMonth->setEnabled(false);
        ui->rbLastMonth->setEnabled(false);
        ui->rbMonth->setEnabled(false);

        // No dates
        ui->rbToday->setEnabled(false);
        ui->rbYesterday->setEnabled(false);
        ui->rbDate->setEnabled(false);

        // no datespans
        ui->rbThisWeek->setEnabled(false);
        ui->rbLastWeek->setEnabled(false);
        ui->rbDateSpan->setEnabled(false);

        // no timespans
        ui->rbTimeSpan->setEnabled(false);

        if (!report.hasCustomCriteria()) {
            ui->pbNext->setText(tr("&Finish"));
        }
        break;
    case Report::TP_Year:
        // years are ok

        // no months
        ui->rbThisMonth->setEnabled(false);
        ui->rbLastMonth->setEnabled(false);
        ui->rbMonth->setEnabled(false);

        // No dates
        ui->rbToday->setEnabled(false);
        ui->rbYesterday->setEnabled(false);
        ui->rbDate->setEnabled(false);

        // no datespans
        ui->rbThisWeek->setEnabled(false);
        ui->rbLastWeek->setEnabled(false);
        ui->rbDateSpan->setEnabled(false);

        // no timespans
        ui->rbTimeSpan->setEnabled(false);
        break;
    case Report::TP_Month:
        // no years
        ui->rbThisYear->setEnabled(false);
        ui->rbLastYear->setEnabled(false);
        ui->rbYear->setEnabled(false);

        // months are ok

        // No dates
        ui->rbToday->setEnabled(false);
        ui->rbYesterday->setEnabled(false);
        ui->rbDate->setEnabled(false);

        // no datespans
        ui->rbThisWeek->setEnabled(false);
        ui->rbLastWeek->setEnabled(false);
        ui->rbDateSpan->setEnabled(false);

        // no timespans
        ui->rbTimeSpan->setEnabled(false);
        break;
    case Report::TP_Day:
        // no years
        ui->rbThisYear->setEnabled(false);
        ui->rbLastYear->setEnabled(false);
        ui->rbYear->setEnabled(false);

        // no months
        ui->rbThisMonth->setEnabled(false);
        ui->rbLastMonth->setEnabled(false);
        ui->rbMonth->setEnabled(false);

        // days are ok

        // no datespans
        ui->rbThisWeek->setEnabled(false);
        ui->rbLastWeek->setEnabled(false);
        ui->rbDateSpan->setEnabled(false);

        // no timespans
        ui->rbTimeSpan->setEnabled(false);
        break;
    case Report::TP_Datespan:
        // years are ok (this is just a datespan from start to end of year)

        // months are ok (this is just a datespan from start to end of month)

        // dates are ok (this is just a datespan of day_1 to day_1)

        // date spans are ok

        // no timespans
        ui->rbTimeSpan->setEnabled(false);
        break;
    case Report::TP_Timespan:
        // Disable nothing!
        break;
    }
}

QDate RunReportDialog::get_date() {
    if (ui->rbToday->isChecked()) {
        return QDate::currentDate();
    } else if (ui->rbYesterday->isChecked()) {
        return QDate::currentDate().addDays(-1);
    } else if (ui->rbDate->isChecked()) {
        return ui->deDate->date();
    }
    return QDate();
}

QDate RunReportDialog::get_month() {
    QDate now = QDate::currentDate();

    QDate this_month;
    this_month.setDate(now.year(), now.month(), 1)    ;

    if (ui->rbThisMonth->isChecked()) {
        return this_month;
    } else if (ui->rbLastMonth->isChecked()) {
        return this_month.addMonths(-1);
    } else if (ui->rbMonth->isChecked()) {
        return ui->deMonth->date();
    }
    return QDate();
}

int RunReportDialog::get_year() {
    int year = QDate::currentDate().year();

    if (ui->rbThisYear->isChecked()) {
        return year;
    } else if (ui->rbLastYear->isChecked()) {
        return year - 1;
    } else if (ui->rbYear->isChecked()) {
        return ui->deYear->date().year();
    }
    return 0;
}

RunReportDialog::date_span_t RunReportDialog::get_date_span() {
    RunReportDialog::date_span_t range;

    if (ui->rbToday->isChecked()
            || ui->rbYesterday->isChecked()
            || ui->rbDate->isChecked()) {
        range.start = get_date();
        range.end = range.start;
    } else if (ui->rbThisMonth->isChecked()
               || ui->rbLastMonth->isChecked()
               || ui->rbMonth->isChecked()) {
        range.start = get_month();
        range.end = range.start.addMonths(1).addDays(-1);
    } else if (ui->rbThisYear->isChecked()
               || ui->rbLastYear->isChecked()
               || ui->rbYear->isChecked()) {
        int year = get_year();
        range.start = QDate(year, 1, 1);
        range.end = range.start.addYears(1).addDays(-1);
    } else if (ui->rbThisWeek->isChecked()
               || ui->rbLastWeek->isChecked()) {

        QDate today = QDate::currentDate();
        int subtractDays = 1 - today.dayOfWeek();
        int addDays = 7 - today.dayOfWeek();
        range.start = today.addDays(subtractDays);
        range.end = today.addDays(addDays);

        if (ui->rbLastWeek->isChecked()) {
            range.start = range.start.addDays(-7);
            range.end = range.end.addDays(-7);
        }
    } else if (ui->rbDateSpan->isChecked()) {
        range.start = ui->deStartDate->date();
        range.end = ui->deEndDate->date();
    }

    return range;
}

RunReportDialog::time_span_t RunReportDialog::get_time_span() {
    time_span_t time_span;

    if (ui->rbToday->isChecked()
            || ui->rbYesterday->isChecked()
            || ui->rbDate->isChecked()
            || ui->rbThisMonth->isChecked()
            || ui->rbLastMonth->isChecked()
            || ui->rbMonth->isChecked()
            || ui->rbThisYear->isChecked()
            || ui->rbLastYear->isChecked()
            || ui->rbYear->isChecked()
            || ui->rbThisWeek->isChecked()
            || ui->rbLastWeek->isChecked()
            || ui->rbDateSpan->isChecked()) {
        date_span_t span = get_date_span();

        time_span.start = QDateTime(span.start, QTime(0, 0));
        time_span.end = QDateTime(span.end, QTime(23, 59, 59, 59));
    } else {
        time_span.start = ui->teStartTime->dateTime();
        time_span.end = ui->teEndTime->dateTime();
    }
    return time_span;
}

void RunReportDialog::moveNextPage() {
    if (nextPage == Page_Finish) {
        runReport();
        accept();
    } else {
        switchPage(nextPage);
    }
}

void RunReportDialog::movePreviousPage() {
    switchPage(previousPage);
}

void RunReportDialog::cancel() {
    reject();
}

void RunReportDialog::switchPage(Page page) {
    if (page == Page_None) {
        return;
    }

    ui->stackedWidget->setCurrentIndex(page);

    // Set previous page
    ui->pbBack->setEnabled(true);
    switch(page) {
    case Page_Timespan:
        previousPage = Page_ReportSelect;
        ui->pbNext->setEnabled(false);
        break;
    case Page_Criteria:
        if (report.timePickerType() == Report::TP_None) {
            previousPage = Page_ReportSelect;
        } else {
            previousPage = Page_Timespan;
        }
        break;
    case Page_None:
    case Page_ReportSelect:
    default:
        previousPage = Page_None;
        ui->pbBack->setEnabled(false);;
        break;
    }

    // Set next page
    ui->pbNext->setText(tr("&Next >"));
    switch(page) {
    case Page_Timespan:
        if (report.hasCustomCriteria()) {
            nextPage = Page_Criteria;
        } else {
            ui->pbNext->setText(tr("&Finish"));
            nextPage = Page_Finish;
        }
        break;
    case Page_Criteria:
        nextPage = Page_Finish;
        break;
    case Page_None:
    case Page_ReportSelect:
    default:
        if (report.timePickerType() == Report::TP_None) {
            if (report.hasCustomCriteria()) {
                nextPage = Page_Criteria;
            } else {
                nextPage = Page_Finish;
            }
        } else {
            nextPage = Page_Timespan;
        }
        break;
    }
}

void RunReportDialog::runReport() {
    AbstractDataSource *ds;
    if (Settings::getInstance().sampleDataSourceType() == Settings::DS_TYPE_DATABASE)
        ds = new DatabaseDataSource(new DialogProgressListener(this), this);
    else
        ds = new WebDataSource(new DialogProgressListener(this), this);

    // TODO: custom criteria
    if (report.timePickerType() == Report::TP_Timespan) {
        time_span_t ts = get_time_span();
        report.run(ds, ts.start, ts.end);
    } else if (report.timePickerType() == Report::TP_Datespan) {
        date_span_t span = get_date_span();
        report.run(ds, span.start, span.end);
    } else if (report.timePickerType() == Report::TP_Day) {
        report.run(ds, get_date(), false);
    } else if (report.timePickerType() == Report::TP_Month) {
        report.run(ds, get_month(), true);
    } else if (report.timePickerType() == Report::TP_Year) {
        report.run(ds, get_year());
    }
}

void RunReportDialog::timespanSelected() {
    bool enabled = ui->rbToday->isChecked()
            || ui->rbYesterday->isChecked()
            || ui->rbDate->isChecked()
            || ui->rbThisMonth->isChecked()
            || ui->rbLastMonth->isChecked()
            || ui->rbMonth->isChecked()
            || ui->rbThisYear->isChecked()
            || ui->rbLastYear->isChecked()
            || ui->rbYear->isChecked()
            || ui->rbThisWeek->isChecked()
            || ui->rbLastWeek->isChecked()
            || ui->rbDateSpan->isChecked()
            || ui->rbTimeSpan->isChecked();

    ui->pbNext->setEnabled(enabled);
}
