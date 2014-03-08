#include "configwizard_private.h"
#include "constants.h"

/*
 * Wizard pages
 */
#include "intropage.h"
#include "accesstypepage.h"
#include "databasedetailspage.h"
#include "selectstationpage.h"
#include "internetsiteinfopage.h"
#include "confirmdetailspage.h"
#include "serverdetailspage.h"

// UI stuff
#include <QPushButton>

// Misc
#include "settings.h"

ConfigWizard::ConfigWizard(QWidget *parent) :
    QWizard(parent)
{
    /* Intro Pages */
    setPage(Page_Intro, new IntroPage);
    setPage(Page_AccessType, new AccessTypePage);

    /* Local Pages */
    setPage(Page_DatabaseDetails, new DatabaseDetailsPage);

    /* Internet Pages */
    setPage(Page_InternetSiteInfo, new InternetSiteInfoPage);
    setPage(Page_ServerDetails, new ServerDetailsPage);

    /* Shared Pages */
    setPage(Page_SelectStation, new SelectStationPage);
    setPage(Page_ConfirmDetails, new ConfirmDetailsPage);


    setStartId(Page_Intro);

    // TODO: remove
    setWizardStyle(ModernStyle);

    setPixmap(QWizard::LogoPixmap, QPixmap(LOGO_PIXMAP));

    setWindowTitle(tr("Configuration Wizard - zxweather Desktop"));
    setWindowIcon(QIcon(":/icons/settings"));

    // This is used on some wizard pages which have 'subpages'.
    subpageBack = new QPushButton(this);
    subpageBack->setText(button(BackButton)->text());
    setButton(CustomButton1, subpageBack);
    setOption(HaveCustomButton1);
    subpageBack->setVisible(false);
}

void ConfigWizard::accept() {

    Settings& settings = Settings::getInstance();

    bool isLocal = field(LOCAL_ACCESS_TYPE_FIELD).toBool();

    // Store the station code.
    bool multipleStationsPresent = field(MULTIPLE_STATIONS_AVAILABLE_FIELD).toBool();
    QString stationCode;
    if (multipleStationsPresent) {
        stationCode = field(SELECTED_STATION_CODE).toString();
    } else {
        DbUtil::StationInfo station = field(FIRST_STATION_FIELD).value<DbUtil::StationInfo>();
        stationCode = station.code;
    }
    settings.setStationCode(stationCode);

    if (isLocal) {
        // save database settings.
        QString dbHostname = field(DATABASE_HOSTNAME_FIELD).toString();
        int dbPort = field(DATABASE_PORT_FIELD).toInt();
        QString dbUser = field(DATABASE_USERNAME_FIELD).toString();
        QString dbPassword = field(DATABASE_PASSWORD_FIELD).toString();
        QString dbName = field(DATABASE_FIELD).toString();

        settings.setLiveDataSourceType(Settings::DS_TYPE_DATABASE);
        settings.setSampleDataSourceType(Settings::DS_TYPE_DATABASE);
        settings.setDatabaseName(dbName);
        settings.setDatabaseHostname(dbHostname);
        settings.setDatabasePort(dbPort);
        settings.setDatabaseUsername(dbUser);
        settings.setDatabasePassword(dbPassword);
    } else {
        // save web interface settings.

        QString webUrl = field(BASE_URL_FIELD).toString();
        bool serverAvailable = field(SERVER_AVAILABLE).toBool();
        QString serverHostname = field(SERVER_HOSTNAME).toString();
        int serverPort = field(SERVER_PORT).toInt();

        settings.setSampleDataSourceType(Settings::DS_TYPE_WEB_INTERFACE);
        settings.setWebInterfaceUrl(webUrl);

        if (!serverAvailable) {
            settings.setLiveDataSourceType(Settings::DS_TYPE_WEB_INTERFACE);
        } else {
            // Configure the weather server
            settings.setLiveDataSourceType(Settings::DS_TYPE_SERVER);
            settings.setServerHostname(serverHostname);
            settings.setServerPort(serverPort);
        }
    }

    QDialog::accept();
}
