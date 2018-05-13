#include "reportdisplaywindow.h"

#include <QGridLayout>
#include <QTabWidget>
#include <QPushButton>
#include <QTextBrowser>
#include <QTableView>
#include <QSqlQueryModel>
#include <QShortcut>
#include <QApplication>
#include <QClipboard>
#include <QFontDatabase>

ReportDisplayWindow::ReportDisplayWindow(QString reportName, QIcon reportIcon, QWidget *parent) : QDialog(parent)
{
    setWindowTitle(reportName);
    setWindowIcon(reportIcon);
    setWindowFlags(Qt::Window);
    resize(800,600);

    QGridLayout *layout = new QGridLayout(this);
    tabs = new QTabWidget(this);
    layout->addWidget(tabs, 0, 0, 1, 2);

    QPushButton *saveButton = new QPushButton(tr("&Save..."), this);
    connect(saveButton, SIGNAL(clicked(bool)), this, SLOT(saveReport()));
    layout->addWidget(saveButton, 1, 1);

    layout->setColumnStretch(0, 1);

    setLayout(layout);
}

void ReportDisplayWindow::addHtmlTab(QString name, QIcon icon, QString content) {
    QWidget *tab = new QWidget(tabs);
    QGridLayout *tabLayout = new QGridLayout(tab);

    QTextBrowser *browser = new QTextBrowser(tab);
    browser->setHtml(content);

    tabLayout->addWidget(browser);
    tab->setLayout(tabLayout);

    if (icon.isNull()) {
        tabs->addTab(tab, name);
    } else {
        tabs->addTab(tab, icon, name);
    }
}

void ReportDisplayWindow::addPlainTab(QString name, QIcon icon, QString text) {
    QWidget *tab = new QWidget(tabs);
    QGridLayout *tabLayout = new QGridLayout(tab);

    QTextBrowser *browser = new QTextBrowser();
    browser->setPlainText(text);
    browser->setFont(QFontDatabase::systemFont(QFontDatabase::FixedFont));

    tabLayout->addWidget(browser);
    tab->setLayout(tabLayout);

    if (icon.isNull()) {
        tabs->addTab(tab, name);
    } else {
        tabs->addTab(tab, icon, name);
    }
}

void ReportDisplayWindow::addGridTab(QString name, QIcon icon, QAbstractTableModel *model) {
    QWidget *tab = new QWidget(tabs);
    QGridLayout *tabLayout = new QGridLayout(tab);

    QTableView *table = new QTableView(tab);
    model->setParent(table);
    table->setModel(model);

    // Setup a keyboard shortcut for copying a selection in the
    // grid to tab-delimited data in the clipboard
    QShortcut *sh = new QShortcut(QKeySequence::Copy,
                                  table,
                                  NULL,
                                  NULL,
                                  Qt::WidgetWithChildrenShortcut);
    sh->setAutoRepeat(false);
    connect(sh, SIGNAL(activated()), this, SLOT(copyGridSelection()));
    sh->setEnabled(true);

    tabLayout->addWidget(table);
    tab->setLayout(tabLayout);

    if (icon.isNull()) {
        tabs->addTab(tab, name);
    } else {
        tabs->addTab(tab, icon, name);
    }
}

void ReportDisplayWindow::setSaveOutputs(QList<report_output_file_t> outputs) {
    this->outputs = outputs;
}

void ReportDisplayWindow::copyGridSelection() {
    // Copies selected data in the table to tab-delimited data on the clipboard

    QObject *sender = QObject::sender();
    QTableView *tableView = qobject_cast<QTableView*>(sender);
    if (tableView == NULL) {
        sender = sender->parent();
        tableView = qobject_cast<QTableView*>(sender);
        if (tableView == NULL) {
            return; // Shortcut came from something other than a table view?
        }
    }

    QAbstractItemModel *tableModel = tableView->model();
    QItemSelectionModel * selectionModel = tableView->selectionModel();
    QModelIndexList selectedIndexes = selectionModel->selectedIndexes();

    qSort(selectedIndexes);

    if(selectedIndexes.size() < 1) {
        return;
    }

    QString clipboardData;

    QModelIndex previous = selectedIndexes.first();

    selectedIndexes.removeFirst();

    for(int i = 0; i < selectedIndexes.size(); i++) {
        QVariant data = tableModel->data(previous);
        QString text = data.toString();

        QModelIndex index = selectedIndexes.at(i);
        clipboardData.append(text);

        if(index.row() != previous.row()) {
            clipboardData.append('\n');
        }
        else {
            clipboardData.append('\t');
        }
        previous = index;
    }

    clipboardData.append(tableModel->data(selectedIndexes.last()).toString());
    clipboardData.append('\n');

    QApplication::clipboard()->setText(clipboardData);
}

void ReportDisplayWindow::saveReport() {
    Report::saveReport(outputs);
}
