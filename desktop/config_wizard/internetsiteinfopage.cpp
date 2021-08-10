#include "internetsiteinfopage.h"
#include "constants.h"
#include "configwizard_private.h"

#include "json/json.h"

#include <QLineEdit>
#include <QFormLayout>
#include <QProgressBar>
#include <QVBoxLayout>
#include <QLabel>
#include <QStackedLayout>
#include <QTcpSocket>
#include <QNetworkAccessManager>
#include <QAbstractButton>
#include <QApplication>
#include <QNetworkRequest>
#include <QNetworkReply>

/****************************************************************************
*************************** INTERNET SITE INFO PAGE *************************
*****************************************************************************
* > Intro > Access Type [REMOT] > Internet Site Info
*
* Obtains the details of the zxweather web interface. Like the Database
* Details Page this page will attempt to connect to the server to verify
* the details are correct.
*
****************************************************************************/
InternetSiteInfoPage::InternetSiteInfoPage(QWidget* parent)
    : QWizardPage(parent)
{
    validated = false;
    port = -1;
    multipleStationsPresent = false;
    serverAvailable = false;

    /*** Details page ***/
    baseUrlEdit = new QLineEdit("http://", this);

    registerField(BASE_URL_FIELD "*", baseUrlEdit);

    QFormLayout *detailsLayout = new QFormLayout(this);
    detailsLayout->addRow(tr("&Web Interface Base URL:"), baseUrlEdit);

    QWidget *detailsPage = new QWidget;
    detailsPage->setLayout(detailsLayout);

    /*** Progress page ***/
    QProgressBar* progressBar = new QProgressBar;
    progressBar->setMinimum(0);
    progressBar->setMaximum(0);
    progressBar->setTextVisible(false);
    progress = new QLabel;
    progress->setText(tr("Connecting..."));
    progress->setAlignment(Qt::AlignHCenter);

    QVBoxLayout *progressPageLayout = new QVBoxLayout;
    progressPageLayout->addStretch(1);
    progressPageLayout->addWidget(progressBar);
    progressPageLayout->addWidget(progress);
    progressPageLayout->addStretch(1);

    QWidget* progressPage = new QWidget;
    progressPage->setLayout(progressPageLayout);

    /*** Error page ***/
    errorLabel = new QLabel;
    errorLabel->setWordWrap(true);

    QVBoxLayout *errorPageLayout = new QVBoxLayout;
    errorPageLayout->addWidget(errorLabel);

    QWidget* errorPage = new QWidget;
    errorPage->setLayout(errorPageLayout);

    /*** Wizard Page ***/
    stackedLayout = new QStackedLayout;
    stackedLayout->addWidget(detailsPage);
    stackedLayout->addWidget(progressPage);
    stackedLayout->addWidget(errorPage);
    setLayout(stackedLayout);

    stackedLayout->setCurrentIndex(SL_DetailsPage);
    setTitle(tr("Site Information"));
    setSubTitle(tr(ISI_DETAIL_SUBTITLE));

    stationLister.reset(new ServerStationLister());

    connect(stationLister.data(), SIGNAL(statusUpdate(QString)),
            progress, SLOT(setText(QString)));
    connect(stationLister.data(), SIGNAL(error(QString)),
            this, SLOT(StationListError(QString)));
    connect(stationLister.data(), SIGNAL(finished(QStringList)),
            this, SLOT(StationListFinished(QStringList)));

    netManager = new QNetworkAccessManager(this);
    connect(netManager, SIGNAL(finished(QNetworkReply*)),
            this, SLOT(requestFinished(QNetworkReply*)));

    registerField(SERVER_STATION_AVAILABILITY, this, "serverStationStatus");
}

InternetSiteInfoPage::~InternetSiteInfoPage() {
    while (!serverStationAvailability.isEmpty())
        delete serverStationAvailability.takeFirst();
}

QVariant InternetSiteInfoPage::getStationStatus() const {
    return QVariant::fromValue(serverStationAvailability);
}

void InternetSiteInfoPage::initializePage() {
    connect(wizard()->button(QWizard::CustomButton1), SIGNAL(clicked()),
            this, SLOT(subpageBack()));
}

void InternetSiteInfoPage::cleanupPage() {
    QWizardPage::cleanupPage();
    disconnect(this, SLOT(subpageBack()));
}

int InternetSiteInfoPage::nextId() const {

    // Get the user to pick which station they want
    if (multipleStationsPresent)
        return ConfigWizard::Page_SelectStation;

    // There is only one station available. And we don't
    // have any server configuration for it. Prompt for the details.
    if (!serverAvailable)
        return ConfigWizard::Page_ServerDetails;

    // There is only one station available. It has valid server
    // configuration data.

    return ConfigWizard::Page_ConfirmDetails;
}

void InternetSiteInfoPage::switchToSubPage(SubPage subPage) {
    wizard()->button(QWizard::BackButton)->setVisible(false);
    wizard()->button(QWizard::CustomButton1)->setVisible(true);
    wizard()->button(QWizard::CustomButton1)->setEnabled(true);

    if (subPage == SL_DetailsPage) {
        qDebug() << "Subpage: Site Information";
        setTitle(tr("Site Information"));
        setSubTitle(tr(ISI_DETAIL_SUBTITLE));
        wizard()->button(QWizard::BackButton)->setVisible(true);
        wizard()->button(QWizard::CustomButton1)->setVisible(false);
        wizard()->button(QWizard::NextButton)->setEnabled(true);
    } else if (subPage == SL_ProgressPage) {
        qDebug() << "Subpage: Progress...";
        setTitle(tr("Downloading Configuration Data"));
        setSubTitle(tr("The configuration wizard is downloading and checking "
                       "configuration data for the remote weather site."));
        wizard()->button(QWizard::CustomButton1)->setEnabled(false);
        wizard()->button(QWizard::CustomButton1)->setVisible(true);
        wizard()->button(QWizard::NextButton)->setEnabled(false);
    } else if (subPage == SL_Error) {
        qDebug() << "Subpage: Error";
        wizard()->button(QWizard::NextButton)->setEnabled(false);
        wizard()->button(QWizard::CustomButton1)->setFocus();
    }

    if (stackedLayout != NULL)
        stackedLayout->setCurrentIndex(subPage);

    currentPage = subPage;
    qApp->processEvents();
}

void InternetSiteInfoPage::subpageBack() {
    qDebug() << "Subpage back.";
    switchToSubPage(SL_DetailsPage);
}

void InternetSiteInfoPage::showErrorPage(QString title, QString subtitle, QString message) {

    switchToSubPage(SL_Error);

    setTitle(title);
    setSubTitle(subtitle);

    errorLabel->setText(message);
    qDebug() << "Error page:" << message;
}

bool InternetSiteInfoPage::validatePage() {

    // We've already been validated. Time to go.
    if (validated) {
        wizard()->button(QWizard::BackButton)->setVisible(true);
        wizard()->button(QWizard::CustomButton1)->setVisible(false);
        wizard()->button(QWizard::NextButton)->setEnabled(true);
        validated = false;
        return true;
    }

    switchToSubPage(SL_ProgressPage);

    progress->setText(tr("Downloading system configuration..."));

    serverStationAvailability.clear();
    stations.clear();

    QString url = baseUrlEdit->text();
    if (!url.endsWith("/"))
        url += "/";
    url += "data/sysconfig.json";

    qDebug() << "Download sysconfig URL:" << url;

    QNetworkRequest request;
    request.setUrl(QUrl(url));
    request.setRawHeader("User-Agent", Constants::USER_AGENT);

    netManager->get(request);

    return false;
}

bool compareStationInfo(const DbUtil::StationInfo &si1,
                        const DbUtil::StationInfo &si2) {
    return si1.sortOrder < si2.sortOrder;
}

void InternetSiteInfoPage::requestFinished(QNetworkReply *reply) {    
    qDebug() << "HTTP Response received";
    if (reply->error() == QNetworkReply::NoError) {
        // All ok.
        QString data = reply->readAll();

        using namespace QtJson;

        bool ok;

        QVariantMap result = Json::parse(data, ok).toMap();

        foreach(QVariant stn, result.value("stations").toList()) {
            DbUtil::StationInfo stationInfo;
            QVariantMap station = stn.toMap();

            stationInfo.code = station.value("code").toString();
            stationInfo.title = station.value("name").toString();
            stationInfo.description = station.value("desc").toString();
            stationInfo.sortOrder = station.value("order").toInt();

            QVariantMap hwType = station.value("hw_type").toMap();

            stationInfo.stationTypeCode = hwType.value("code").toString();
            stationInfo.stationTypeName = hwType.value("name").toString();
            qDebug() << "Found Station:" << stationInfo.title;
            stations.append(stationInfo);
        }

        std::sort(stations.begin(), stations.end(), compareStationInfo);

        if (result.contains("zxweatherd_host"))
            serverHostname = result.value("zxweatherd_host").toString();
        if (result.contains("zxweatherd_raw_port"))
            port = result.value("zxweatherd_raw_port").toInt();

        serverAvailable = !serverHostname.isNull() && port != -1;

        multipleStationsPresent = stations.count() > 1;
        stationList = QVariant::fromValue(stations);
        firstStation = QVariant::fromValue(stations.first());

        // Pass available stations through to the ConfirmDetailsPage.
        setField(STATION_LIST_FIELD, stationList);
        setField(FIRST_STATION_FIELD, firstStation);
        setField(MULTIPLE_STATIONS_AVAILABLE_FIELD, multipleStationsPresent);

        foreach (DbUtil::StationInfo stn, stations) {
            ServerStation* svrStn = new ServerStation;
            svrStn->code = stn.code;
            svrStn->available = false;
            svrStn->hostname = serverHostname;
            svrStn->port = port;
            serverStationAvailability.append(svrStn);
        }

        if (!serverAvailable) {
            // No zxweather server available as far as we can tell here.
            // Nothing much more to do right now so we'll continue on.
            qDebug() << "No server available";

            validationComplete();
            return;
        }

        /*
         * Apparently there is a zxweather server setup! We'll try to connect
         * to it and see what stations it knows about.
         */
        stationLister->fetchStationList(serverHostname, port);

    } else {
        // Error!
        qDebug() << "Error response:" << reply->errorString();
        showErrorPage(tr("Error"),
                      tr("An error occurred while downloading system configuration"),
                      QString(tr("An error occurred while downloading system configuration"
                         " from the remote website. The error was: %1")).arg(reply->errorString()));
    }
}

void InternetSiteInfoPage::validationComplete() {
    wizard()->button(QWizard::BackButton)->setVisible(true);
    wizard()->button(QWizard::CustomButton1)->setVisible(false);
    wizard()->button(QWizard::NextButton)->setEnabled(true);

    switchToSubPage(SL_DetailsPage);

    validated = true;
    wizard()->button(QWizard::NextButton)->setEnabled(true);
    wizard()->button(QWizard::NextButton)->click();
}

void InternetSiteInfoPage::StationListError(QString message) {
    progress->setText(message);
    validationComplete();
}

void InternetSiteInfoPage::StationListFinished(QStringList stations) {

    foreach (ServerStation* stn, serverStationAvailability) {
        if (stations.contains(stn->code))
            stn->available = true;
    }

    /* If there is only one station then pass the server details straight
     * through to the ConfirmDetails page as thats where we're heading
     * next (normally the StationSelectPage would do this if there were
     * multiple stations).
     */
    if (!multipleStationsPresent) {
        // Pass details of the server to the confirm details page.
        ServerStation* stn = serverStationAvailability.first();
        setField(SERVER_AVAILABLE, stn->available);
        setField(SERVER_HOSTNAME, stn->hostname);
        setField(SERVER_PORT, stn->port);
    }

    validationComplete();
}

