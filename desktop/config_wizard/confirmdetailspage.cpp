#include "confirmdetailspage.h"

#include "configwizard_private.h"

// UI stuff
#include <QTableWidget>
#include <QLabel>
#include <QVBoxLayout>
#include <QHeaderView>

// Misc stuff
#include <QtDebug>
#include "dbutil.h"

/****************************************************************************
**************************** CONFIRM DETAILS PAGE ***************************
*****************************************************************************
* > Intro > Access Type [LOCAL] > Database Details > Select Station > Confirm
*
* Displays the users selected options and provides a chance to go back and
* change them if any of it is wrong. This is also the last chance to cancel.
* Once this page has completed the settings will be written to disk.
*
****************************************************************************/

ConfirmDetailsPage::ConfirmDetailsPage(QWidget *parent) : QWizardPage(parent) {

    setTitle(tr("Confirm Details"));

    setPixmap(QWizard::WatermarkPixmap, QPixmap(WATERMARK_PIXMAP));

    table = new QTableWidget;
    table->setRowCount(5);
    table->setColumnCount(2);
    table->setShowGrid(false);
    table->setSelectionBehavior(QAbstractItemView::SelectRows);
    table->setEditTriggers(QTableWidget::NoEditTriggers);
    table->verticalHeader()->setVisible(false);
    table->verticalHeader()->setDefaultSectionSize(15);
    table->horizontalHeader()->setVisible(false);
    table->horizontalHeader()->setStretchLastSection(true);



    QLabel* infoLabel = new QLabel(
                tr("<p>The Configuration Wizard has finished collecting "
                   "connection details. Please review the settings below and "
                   "click <b>Finish</b> if they are correct.</p>"
                   "<p>If required, any of the settings below can be changed later"
                   " from the Data Sources tab of the Settings Dialog.</p>"));

    infoLabel->setWordWrap(true);
    QVBoxLayout *layout = new QVBoxLayout;
    layout->addWidget(infoLabel);
    layout->addWidget(table);

    setLayout(layout);
}

int ConfirmDetailsPage::nextId() const {
    return ConfigWizard::Page_None;
}

void ConfirmDetailsPage::initializePage() {

    bool isLocal = field(LOCAL_ACCESS_TYPE_FIELD).toBool();

    if (isLocal)
        initialiseForDb();
    else
        initialiseForInternet();
}

void ConfirmDetailsPage::initialiseForDb() {
    table->clear();
    table->setItem(0, 0, new QTableWidgetItem(tr("Connection Type:")));
    table->setItem(1, 0, new QTableWidgetItem(tr("Server:")));
    table->setItem(2, 0, new QTableWidgetItem(tr("Username:")));
    table->setItem(3, 0, new QTableWidgetItem(tr("Database:")));
    table->setItem(4, 0, new QTableWidgetItem(tr("Station:")));

    // TODO: check field

    QString dbHostname = field(DATABASE_HOSTNAME_FIELD).toString();
    int dbPort = field(DATABASE_PORT_FIELD).toInt();
    QString dbUser = field(DATABASE_USERNAME_FIELD).toString();
    QString dbName = field(DATABASE_FIELD).toString();



    qDebug() << "DB Hostname:" << dbHostname;
    qDebug() << "DB Port:" << dbPort;
    qDebug() << "DB User:" << dbUser;
    qDebug() << "DB Name:" << dbName;


    table->setItem(0, 1, new QTableWidgetItem(tr("Local (database)")));
    table->setItem(1, 1, new QTableWidgetItem(QString("%1:%2")
                           .arg(dbHostname).arg(dbPort)));
    table->setItem(2, 1, new QTableWidgetItem(dbUser));
    table->setItem(3, 1, new QTableWidgetItem(dbName));

    setStationName();
}

void ConfirmDetailsPage::initialiseForInternet() {
    table->clear();
    table->setItem(0, 0, new QTableWidgetItem(tr("Connection Type:")));
    table->setItem(1, 0, new QTableWidgetItem(tr("Web URL:")));
    table->setItem(2, 0, new QTableWidgetItem(tr("Server Available:")));
    table->setItem(3, 0, new QTableWidgetItem(tr("Server:")));
    table->setItem(4, 0, new QTableWidgetItem(tr("Station:")));

    QString webUrl = field(BASE_URL_FIELD).toString();
    bool multipleStationsPresent = field(MULTIPLE_STATIONS_AVAILABLE_FIELD).toBool();
    bool serverAvailable = field(SERVER_AVAILABLE).toBool();
    QString serverHostname = field(SERVER_HOSTNAME).toString();
    int serverPort = field(SERVER_PORT).toInt();

    qDebug() << "Multiple stations?" << multipleStationsPresent;

    table->setItem(0, 1, new QTableWidgetItem(tr("Internet")));
    table->setItem(1, 1, new QTableWidgetItem(webUrl));
    table->setItem(2, 1, new QTableWidgetItem(serverAvailable ? tr("Yes") : tr("No")));
    if (serverAvailable)
        table->setItem(3, 1, new QTableWidgetItem(QString(tr("%1:%2"))
                                              .arg(serverHostname).arg(serverPort)));
    else
        table->setItem(3, 1, new QTableWidgetItem(tr("n/a")));

    setStationName();
}
void ConfirmDetailsPage::setStationName() {
    bool multipleStationsPresent = field(MULTIPLE_STATIONS_AVAILABLE_FIELD).toBool();
    if (multipleStationsPresent) {
        qDebug() << "Taking value from Select Station Page";
        // There are multiple stations. Get the details set by the station
        // select page.
        QString stationTitle = field(SELECTED_STATION_TITLE).toString();
        QString stationCode = field(SELECTED_STATION_CODE).toString();

        qDebug() << "Selected Station:" << stationTitle << stationCode;

        // The user had to choose a station on the station select screen.
        table->setItem(4, 1, new QTableWidgetItem(
                           QString(tr("%1 (%2)"))
                               .arg(stationTitle).arg(stationCode)));
    } else {
        // Only one station was available. Get the details set by the
        // database details page.
        DbUtil::StationInfo station = field(FIRST_STATION_FIELD).value<DbUtil::StationInfo>();

        table->setItem(4, 1, new QTableWidgetItem(
                           QString(tr("%1 (%2)")).arg(station.title)
                                .arg(station.code)));
    }
}
