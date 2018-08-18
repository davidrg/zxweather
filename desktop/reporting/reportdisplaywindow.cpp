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
#include "sortproxymodel.h"

#include "datasource/samplecolumns.h"
#include "charts/chartwindow.h"

#if (QT_VERSION >= QT_VERSION_CHECK(5,0,0))
#include <QUrlQuery>
#endif

#if (QT_VERSION < QT_VERSION_CHECK(5,0,0))
#include <QFont>
#endif

ReportDisplayWindow::ReportDisplayWindow(QString reportName, QIcon reportIcon, QWidget *parent) : QDialog(parent)
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

void ReportDisplayWindow::addPlainTab(QString name, QIcon icon, QString text) {
    QWidget *tab = new QWidget(tabs);
    QGridLayout *tabLayout = new QGridLayout(tab);

    QTextBrowser *browser = new QTextBrowser();
    browser->setPlainText(text);
    #if (QT_VERSION >= QT_VERSION_CHECK(5,0,0))
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

DataSet decodeDataSet(QUrl url) {
    QString start, end, title, graphs, aggregate, grouping, group_minutes;

#if (QT_VERSION >= QT_VERSION_CHECK(5,0,0))
    QUrlQuery q(url);
    start = q.queryItemValue("start");
    end = q.queryItemValue("end");
    if (q.hasQueryItem("title")) {
        title = q.queryItemValue("title");
    }
    graphs = q.queryItemValue("graphs").toLower();
    if (q.hasQueryItem("aggregate")) {
        aggregate = q.queryItemValue("aggregate").toLower();
        grouping = q.queryItemValue("grouping").toLower();
        if (grouping == "custom") {
            group_minutes = q.queryItemValue("group_minutes").toLower();
        }
    }
#else
    start = url.queryItemValue("start");
    end = url.queryItemValue("end");
    if (url.hasQueryItem("title")) {
        title = url.queryItemValue("title");
    }
    graphs = url.queryItemValue("graphs").toLower();
    if (url.hasQueryItem("aggregate")) {
        aggregate = url.queryItemValue("aggregate").toLower();
        grouping = url.queryItemValue("grouping").toLower();
        if (grouping == "custom") {
            group_minutes = url.queryItemValue("group_minutes").toLower();
        }
    }
#endif

    DataSet ds;
    ds.startTime = QDateTime::fromString(start, Qt::ISODate);
    ds.endTime = QDateTime::fromString(end, Qt::ISODate);
    if (!title.isNull()) {
        ds.title = title;
    }
    QStringList columns = graphs.split("+");
    foreach (QString col, columns) {
        if (col == "temperature") {
            ds.columns |= SC_Temperature;
        } else if (col == "indoor_temperature") {
            ds.columns |= SC_IndoorTemperature;
        } else if (col == "apparent_temperature") {
            ds.columns |= SC_ApparentTemperature;
        } else if (col == "wind_chill") {
            ds.columns |= SC_WindChill;
        } else if (col == "dew_point") {
            ds.columns |= SC_DewPoint;
        } else if (col == "humidity") {
            ds.columns |= SC_Humidity;
        } else if (col == "indoor_humidity") {
            ds.columns |= SC_IndoorHumidity;
        } else if (col == "pressure") {
            ds.columns |= SC_Pressure;
        } else if (col == "rainfall") {
            ds.columns |= SC_Rainfall;
        } else if (col == "average_wind_speed") {
            ds.columns |= SC_AverageWindSpeed;
        } else if (col == "gust_wind_speed") {
            ds.columns |= SC_GustWindSpeed;
        } else if (col == "wind_direction") {
            ds.columns |= SC_WindDirection;
        } else if (col == "solar_radiation") {
            ds.columns |= SC_SolarRadiation;
        } else if (col == "uv_index") {
            ds.columns |= SC_UV_Index;
        } else if (col == "reception") {
            ds.columns |= SC_Reception;
        } else if (col == "high_temperature") {
            ds.columns |= SC_HighTemperature;
        } else if (col == "low_temperature") {
            ds.columns |= SC_LowTemperature;
        } else if (col == "high_rain_rate") {
            ds.columns |= SC_HighRainRate;
        } else if (col == "gust_wind_speed") {
            ds.columns |= SC_GustWindDirection;
        } else if (col == "evapotranspiration") {
            ds.columns |= SC_Evapotranspiration;
        } else if (col == "high_solar_radiation") {
            ds.columns |= SC_HighSolarRadiation;
        } else if (col == "high_uv_index") {
            ds.columns |= SC_HighUVIndex;
        }
    }

    ds.aggregateFunction = AF_None;
    ds.groupType = AGT_None;
    ds.customGroupMinutes = 0;
    if (!aggregate.isNull()) {
        if (aggregate == "none") {
            ds.aggregateFunction = AF_None;
        } else if (aggregate == "average") {
            ds.aggregateFunction = AF_Average;
        } else if (aggregate == "min") {
            ds.aggregateFunction = AF_Minimum;
        } else if (aggregate == "max") {
            ds.aggregateFunction = AF_Maximum;
        } else if (aggregate == "sum") {
            ds.aggregateFunction = AF_Sum;
        } else if (aggregate == "running_total") {
            ds.aggregateFunction = AF_RunningTotal;
        }

        if (grouping == "none") {
            ds.groupType = AGT_None;
        } else if (grouping == "hour") {
            ds.groupType = AGT_Hour;
        } else if (grouping == "day") {
            ds.groupType = AGT_Day;
        } else if (grouping == "month") {
            ds.groupType = AGT_Month;
        } else if (grouping == "year") {
            ds.groupType = AGT_Year;
        } else if (grouping == "custom") {
            ds.groupType = AGT_Custom;
            ds.customGroupMinutes = group_minutes.toInt();
        }
    }
    return ds;
}

void ReportDisplayWindow::linkClicked(QUrl url) {
    qDebug() << "Url" << url;
    if (url.scheme() == "zxw") {
        if (url.authority() == "plot") {
            QList<DataSet> ds;
            ds.append(decodeDataSet(url));

            ChartWindow *cw = new ChartWindow(ds, solarDataAvailable, wirelessAvailable);
            cw->setAttribute(Qt::WA_DeleteOnClose);
            cw->show();
            return;
        }
    }

    QDesktopServices::openUrl(url);
}
