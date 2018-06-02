#include "accesstypepage.h"

#include "configwizard_private.h"

#include <QRadioButton>
#include <QLabel>
#include <QVBoxLayout>
#include <QSqlDatabase>

/****************************************************************************
****************************** ACCESS TYPE PAGE *****************************
*****************************************************************************
* > Intro > Access Type
*
* Allows the user to choose how they will access weather data - either from
* a weather database on the local network or remotely via the zxweather web
* interface.
*
****************************************************************************/

AccessTypePage::AccessTypePage(QWidget *parent)
    : QWizardPage(parent)
{
    setTitle(tr("Access Type"));
    setSubTitle(tr("Choose how you will access weather data."));

    topLabel = new QLabel(tr("<p>You have two ways to access your weather data:"
                             "<ul>"
                             "<li><b>Local:</b> Retrieve weather data "
                             "directly from a database on your local network."
                             " This is always the fastest option but requires a "
                             "username, password and other details for your "
                             "database server.</li>"
                             "<li><b>Internet:</b> Retrieve weather data from "
                             "the internet. You only need the URL for the "
                             "zxweather web interface to set this up and you "
                             "can use it anywhere in the world. The down side "
                             "is producing charts and exporting data is slower"
                             " and the latest data may not always be "
                             "available.<ul></p><p></p>"));
    topLabel->setWordWrap(true);
    optionHeading = new QLabel(tr("Access Type:"));
    rbLocal = new QRadioButton(tr("&Local"));

    if (!QSqlDatabase::drivers().contains("QPSQL")) {
        rbLocal->setEnabled(false);
        rbLocal->setText("&Local (PostgreSQL database driver not found)");
    }

#ifdef NO_ECPG
    rbLocal->setEnabled(false);
    rbLocal->setText("&Local (PostgreSQL live data support disabled at build time)");
#endif

    rbLocal->setWhatsThis(tr("Access data from your local weather database. "
                             "You will need the database name, hostname, port,"
                             " username and password. This is the fastest "
                             "option."));
    connect(rbLocal, SIGNAL(clicked()), this, SLOT(completenessChanged()));

    rbInternet = new QRadioButton(tr("&Internet"));
    rbInternet->setWhatsThis(tr("Access data over the internet. Charts and "
                                "data exports will be slower but you only "
                                "need the web interface URL to set it up."));
    connect(rbInternet, SIGNAL(clicked()), this, SLOT(completenessChanged()));

    QVBoxLayout *layout = new QVBoxLayout;
    layout->addWidget(topLabel);
    layout->addWidget(optionHeading);
    layout->addWidget(rbLocal);
    layout->addWidget(rbInternet);

    registerField(LOCAL_ACCESS_TYPE_FIELD, rbLocal);

    registerField(MULTIPLE_STATIONS_AVAILABLE_FIELD,
                  this, "multipleStationsArePresent");
    registerField(STATION_LIST_FIELD, this, "stationList");
    registerField(FIRST_STATION_FIELD, this, "station");

    registerField(SERVER_AVAILABLE, this, "serverAvailable");
    registerField(SERVER_HOSTNAME, this, "serverHostname");
    registerField(SERVER_PORT, this, "serverPort");

    setLayout(layout);
}

int AccessTypePage::nextId() const {
    if (rbInternet->isChecked())
        return ConfigWizard::Page_InternetSiteInfo;

    return ConfigWizard::Page_DatabaseDetails;
}

bool AccessTypePage::isComplete() const {
    return rbLocal->isChecked() || rbInternet->isChecked();
}

void AccessTypePage::completenessChanged() {
    emit completeChanged();
}
