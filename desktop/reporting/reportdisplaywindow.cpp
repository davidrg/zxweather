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
#include <QDesktopServices>
#include <QHeaderView>
#include <QDate>
#include <QtDebug>
#include <algorithm>

#include "sortproxymodel.h"
#include "datasource/samplecolumns.h"

#include "abstracturlhandler.h"

#if (QT_VERSION >= QT_VERSION_CHECK(5,0,0))
#include <QUrlQuery>
#endif

#if (QT_VERSION < QT_VERSION_CHECK(5,0,0))
#include <QFont>
#endif

ReportDisplayWindow::ReportDisplayWindow(QString reportName, QIcon reportIcon, AbstractUrlHandler *urlHandler, QWidget *parent) : QDialog(parent)
{
    setWindowTitle(reportName);
    setWindowIcon(reportIcon);
    setWindowFlags(Qt::Window);
    resize(700,600*1.414);

    QGridLayout *layout = new QGridLayout(this);
    tabs = new QTabWidget(this);
    layout->addWidget(tabs, 0, 0, 1, 2);

    QPushButton *saveButton = new QPushButton(tr("&Save..."), this);
    connect(saveButton, SIGNAL(clicked(bool)), this, SLOT(saveReport()));
    layout->addWidget(saveButton, 1, 1);

    layout->setColumnStretch(0, 1);

    setLayout(layout);

    this->urlHandler = urlHandler;
}

void ReportDisplayWindow::addHtmlTab(QString name, QIcon icon, QString content) {
    QWidget *tab = new QWidget(tabs);
    QGridLayout *tabLayout = new QGridLayout(tab);

    QTextBrowser *browser = new QTextBrowser(tab);
    browser->setHtml(content);
    browser->setOpenLinks(false);
    //browser->setOpenExternalLinks(true);
    connect(browser, SIGNAL(anchorClicked(QUrl)), this, SLOT(linkClicked(QUrl)));

    tabLayout->addWidget(browser);
    tab->setLayout(tabLayout);

    if (icon.isNull()) {
        tabs->addTab(tab, name);
    } else {
        tabs->addTab(tab, icon, name);
    }
}

void ReportDisplayWindow::addPlainTab(QString name, QIcon icon, QString text, bool wordWrap) {
    QWidget *tab = new QWidget(tabs);
    QGridLayout *tabLayout = new QGridLayout(tab);

    QTextBrowser *browser = new QTextBrowser();
    browser->setPlainText(text);
    browser->setWordWrapMode(wordWrap ? QTextOption::WrapAtWordBoundaryOrAnywhere : QTextOption::NoWrap);
    #if (QT_VERSION >= QT_VERSION_CHECK(5,2,0))
    browser->setFont(QFontDatabase::systemFont(QFontDatabase::FixedFont));
    #else
    QFont font = browser->document()->defaultFont();
    font.setFamily("Courier New");
    browser->document()->setDefaultFont(font);
    #endif

    tabLayout->addWidget(browser);
    tab->setLayout(tabLayout);

    if (icon.isNull()) {
        tabs->addTab(tab, name);
    } else {
        tabs->addTab(tab, icon, name);
    }
}

void ReportDisplayWindow::addGridTab(QString name, QIcon icon, QAbstractTableModel *model, QStringList hideColumns) {
    QWidget *tab = new QWidget(tabs);
    QGridLayout *tabLayout = new QGridLayout(tab);

    QTableView *table = new QTableView(tab);
    model->setParent(table);
    //QSortFilterProxyModel *sortableModel = new QSortFilterProxyModel(this);
    SortProxyModel *sortableModel = new SortProxyModel(this);
    sortableModel->setSourceModel(model);
    table->setModel(sortableModel);
    table->resizeColumnsToContents();
    table->setSortingEnabled(true);

    table->verticalHeader()->setDefaultSectionSize(23);
    table->verticalHeader()->setMinimumSectionSize(23);

    foreach (QString col, hideColumns) {
        for(int i = 0; i < table->model()->columnCount(); i++) {
            if (table->model()->headerData(i, Qt::Horizontal, Qt::DisplayRole).toString() == col) {
                table->hideColumn(i);
            }
        }
    }

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

    std::sort(selectedIndexes.begin(), selectedIndexes.end());

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



void ReportDisplayWindow::linkClicked(QUrl url) {
    qDebug() << "Url" << url;
    if (url.scheme() == "zxw") {
        urlHandler->handleUrl(url, solarDataAvailable, wirelessAvailable);
        return;
    }

    QDesktopServices::openUrl(url);
}
