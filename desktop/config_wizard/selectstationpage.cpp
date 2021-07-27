#include "selectstationpage.h"

#include "configwizard_private.h"

// UI stuff
#include <QDialogButtonBox>
#include <QTableWidget>
#include <QLayout>
#include <QSignalMapper>
#include <QVBoxLayout>
#include <QLabel>
#include <QRadioButton>
#include <QTextBrowser>

// Misc stuff
#include <QtDebug>

#include "internetsiteinfopage.h"

#ifdef SINGLE_INSTANCE
#include "applock.h"
#include "constants.h"
#endif

/****************************************************************************
***************************** SELECT STATION PAGE ***************************
*****************************************************************************
* > Intro > Access Type [LOCAL] > Database Details > Select Station
* > Intro > Access Type [REMOT] > Server Details   > Select Station
*
* This page is shown when there is data from multiple stations available. It
* allows the user to select which weather station to subscribe to.
*
****************************************************************************/

SelectStationPage::SelectStationPage(QWidget *parent): QWizardPage(parent) {
    optionListWidget = 0;
    isLocal = false;
    serverAvailable = false;

    setTitle(tr("Select Weather Station"));
    setSubTitle(tr("There are multiple weather stations available. Select the"
                   " one you wish to use."));

    optionMapper = new QSignalMapper(this);
    connect(optionMapper, SIGNAL(mapped(QWidget*)),
            this, SLOT(stationRadioButtonClick(QWidget*)));

    mainLayout = new QVBoxLayout;

    mainLayout->addWidget(new QLabel(tr("Weather Station:")));

    setLayout(mainLayout);

    registerField(SELECTED_STATION_CODE, this, "stationCode");
    registerField(SELECTED_STATION_TITLE, this, "stationTitle");
}

void SelectStationPage::initializePage() {

    isLocal = field(LOCAL_ACCESS_TYPE_FIELD).toBool();

    /*
     * If we've already been on the select station page before
     * then we'll need to clean out the list of stations on
     * the page so we don't make a mess.
     */
    if (optionListWidget != 0) {
        mainLayout->removeWidget(optionListWidget);
        delete optionListWidget;
        optionListWidget = 0;
    }

    optionListWidget = new QWidget;
    mainLayout->addWidget(optionListWidget);

    QVBoxLayout *mainLayout = new QVBoxLayout;

    QVariant serialized = field(STATION_LIST_FIELD);

    QList<DbUtil::StationInfo> stations = serialized.value<QList<DbUtil::StationInfo> >();

    qDebug() << "Stations:" << stations.count();

    foreach(DbUtil::StationInfo station, stations) {
        stationInfoByCode[station.code] = station;

        QLayout* option = createStationOption(station);
        mainLayout->addLayout(option);
    }

    optionListWidget->setLayout(mainLayout);

    qDebug() << "Current Station:" << selectedStationCode << selectedStationTitle;
}

QLayout* SelectStationPage::createStationOption(DbUtil::StationInfo info) {
    QHBoxLayout *layout = new QHBoxLayout;

    QRadioButton *rb = new QRadioButton(QString(tr("%1 - %2"))
                                            .arg(info.code)
                                            .arg(info.title),
                                        this);

    qDebug() << "Create option" << info.code;

    rb->setProperty("stationCode", info.code);
    rb->setProperty("stationTitle", info.title);

#ifdef SINGLE_INSTANCE
    // Check nothing else is already using this station. We only allow one
    // zxweather instance per station.
    AppLock lock;
    lock.lock(Constants::SINGLE_INSTANCE_LOCK_PREFIX + info.code.toLower());
    if (lock.isRunning()) {
        rb->setEnabled(false);
        rb->setToolTip(tr("Another instance of zxweather is already connected to this weather station"));
    }
#endif

    QLabel *details = new QLabel(
                QString("<a href=\"%1\">%2</a>").arg(
                    info.code).arg(tr("More information...")),
                this);

    connect(rb, SIGNAL(clicked()), optionMapper, SLOT(map()));
    optionMapper->setMapping(rb, rb);

    connect(details, SIGNAL(linkActivated(QString)),
            this, SLOT(stationDetailsClick(QString)));

    layout->addWidget(rb);
    layout->addWidget(details);
    layout->addStretch(1);
    return layout;
}

void SelectStationPage::stationRadioButtonClick(QWidget *object) {
    QRadioButton* rb = qobject_cast<QRadioButton*>(object);
    if (rb == 0) {
        qWarning() << "Expected a QRadioButton* but received something else in"
                      " SelectStationPage::stationRadioButtonClick(QObject*).";
        return;
    }

    QString code = rb->property("stationCode").toString();

    if (rb->isChecked()) {
        selectedStationCode = code;
        selectedStationTitle = rb->property("stationTitle").toString();
        qDebug() << "Station selected:" << selectedStationCode;

        setField(MULTIPLE_STATIONS_AVAILABLE_FIELD, true);

        emit completeChanged();

        if (!isLocal) {
            QList<ServerStation*> serverStationStatus = field(SERVER_STATION_AVAILABILITY).value<QList<ServerStation*> >();

            serverAvailable = false;
            QString serverHostname;
            int serverPort = 0;

            foreach (ServerStation* stn, serverStationStatus) {
                qDebug() << "Checking code" << stn->code;
                if (stn->code == selectedStationCode) {
                    serverAvailable = stn->available;
                    serverHostname = stn->hostname;
                    serverPort = stn->port;
                    qDebug() << "Server Available?" << serverAvailable;
                }
            }

            // Pass details of the server to the confirm details page.
            setField(SERVER_AVAILABLE, serverAvailable);
            setField(SERVER_HOSTNAME, serverHostname);
            setField(SERVER_PORT, serverPort);
        }
    } else {
        qDebug() << "Ignore click (not checked):" << code;
    }
}

bool SelectStationPage::isComplete() const {
    if (selectedStationCode == "")
        return false;
    return true;
}

void SelectStationPage::stationDetailsClick(QString code) {
    StationInfoDialog sid(stationInfoByCode[code], this);
    sid.exec();
}

int SelectStationPage::nextId() const {
    if (!isLocal && !serverAvailable) {
        qDebug() << "Station has no server available. Proceeding to server details page.";
        // No server available. Ask the user if they wish to set one up.
        return ConfigWizard::Page_ServerDetails;
    }

    return ConfigWizard::Page_ConfirmDetails;
}

/****************************************************************************
************************* STATION INFORMATION DIALOG*************************
*****************************************************************************
* <Part of the Select Station Page>
*
* Displays detailed information about a weather station. This is:
*   + Name
*   + Code
*   + Hardware Type
*   + Description
*
****************************************************************************/

StationInfoDialog::StationInfoDialog(DbUtil::StationInfo info, QWidget *parent)
    : QDialog(parent)
{
/*   +-----------------------------------------------------------+
     | Station Information                                     X |
     +-----------------------------------------------------------+
     |                                                           |
     | Name:           foo station                  |/\/\/\/\/\| |
     | Code:           stn1                                      |
     | Hardware Type:  Davis Vantage Vue compatible              |
     | +-------------------------------------------------------+ |
     | |                                                       | |
     | | Station description (text browser widget)             | |
     . .                                                       . .
     . .                                                       . .
     | |                                                       | |
     | +-------------------------------------------------------+ |
     |                                                 [ CLOSE ] |
     +-----------------------------------------------------------+    */

    setWindowTitle(tr("Station Information"));

    QLabel *nameLabel = new QLabel(tr("Name:"), this);
    QLabel *codeLabel = new QLabel(tr("Code:"), this);
    QLabel *hwTypeLabel = new QLabel(tr("Hardware Type:"), this);

    QLabel *name = new QLabel(info.title, this);
    name->setWordWrap(true);
    QLabel *code = new QLabel(info.code, this);
    code->setWhatsThis(tr("A short identifier for the weather station. This is"
                       " used to identify the station within zxweather."));
    QLabel *hwType = new QLabel(info.stationTypeName, this);
    hwType->setWhatsThis(tr("The type of hardware the weather station is using"));
    QTextBrowser *description = new QTextBrowser(this);
    description->setHtml(info.description);
    QDialogButtonBox *buttonBox =
            new QDialogButtonBox(QDialogButtonBox::Close);
    connect(buttonBox, SIGNAL(rejected()), this, SLOT(accept()));

    QGridLayout *layout = new QGridLayout(this);
    layout->addWidget(nameLabel, 0, 0);
    layout->addWidget(name, 0, 1);
    layout->setColumnStretch(2, 1); // horizontal space to push the labels over
    layout->addWidget(codeLabel, 1, 0);
    layout->addWidget(code, 1, 1);
    layout->addWidget(hwTypeLabel, 2, 0);
    layout->addWidget(hwType, 2, 1);
    layout->addWidget(description, 3, 0, 1, 3);
    layout->addWidget(buttonBox, 4, 0, 1, 3);

    setLayout(layout);
}
