#include "serverdetailspage.h"

#include "configwizard_private.h"

#include <QFormLayout>
#include <QGridLayout>
#include <QApplication>
#include <QSpacerItem>
#include "dbutil.h"
#include "configwizard.h"

ServerDetailsPage::ServerDetailsPage(QWidget *parent)
    : QWizardPage(parent)
{
    stackedLayout = 0;
    validated = false;

    hostname = new QLineEdit;
    hostname->setEnabled(false);
    port = new QSpinBox;
    port->setMaximum(65535);
    port->setValue(4224);
    port->setEnabled(false);

    QLabel* infoLabel = new QLabel;
    infoLabel->setText(tr("No weather server was found for your chosen weather station. "
        "The weather server provides instant updates when ever current conditions "
        "change. Without one you will only get updated weather data once every 30 "
        "seconds regardless of how fast the weather station updates. If you know "
        "of a weather server that carries data for your chosen weather station you "
        "can configure it here."));
    infoLabel->setWordWrap(true);

    useServer = new QRadioButton;
    useServer->setText(tr("&Use a weather server"));

    noServer = new QRadioButton;
    noServer->setText(tr("&Don't use a weather server"));

    QFormLayout* serverDetailsLayout = new QFormLayout;
    serverDetailsLayout->addRow(tr("Server &Host Name"), hostname);
    serverDetailsLayout->addRow(tr("Server P&ort"), port);

    QGridLayout* pageLayout = new QGridLayout;
    pageLayout->addWidget(infoLabel, 0, 0, 1, 2);
    pageLayout->addItem(new QSpacerItem(5,10,QSizePolicy::Fixed, QSizePolicy::Fixed), 1, 0);
    pageLayout->addWidget(useServer, 2, 0, 1, 2);
    pageLayout->addItem(new QSpacerItem(20, 5, QSizePolicy::Fixed), 3, 0);
    pageLayout->addLayout(serverDetailsLayout, 3, 1);
    pageLayout->addWidget(noServer, 4, 0, 1, 2);
    pageLayout->addItem(new QSpacerItem(3, 20, QSizePolicy::Minimum, QSizePolicy::Expanding), 5, 0, 1, 2);

    detailsPage = new QWidget;
    detailsPage->setLayout(pageLayout);

    progressBar = new QProgressBar;
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

    progressPage = new QWidget;
    progressPage->setLayout(progressPageLayout);

    errorLabel = new QLabel;
    errorLabel->setWordWrap(true);

    QVBoxLayout *errorPageLayout = new QVBoxLayout;
    errorPageLayout->addWidget(errorLabel);

    errorPage = new QWidget;
    errorPage->setLayout(errorPageLayout);

    stackedLayout = new QStackedLayout;
    stackedLayout->addWidget(detailsPage);
    stackedLayout->addWidget(progressPage);
    stackedLayout->addWidget(errorPage);

    QVBoxLayout *layout = new QVBoxLayout;
    layout->addLayout(stackedLayout);

    setLayout(layout);

    stackedLayout->setCurrentIndex(SL_DetailsPage);
    setTitle(tr("Server Information"));
    setSubTitle(tr("Enter the connection details for the weather server."));

    stationLister.reset(new ServerStationLister());

    connect(stationLister.data(), SIGNAL(statusUpdate(QString)),
            progress, SLOT(setText(QString)));
    connect(stationLister.data(), SIGNAL(error(QString)),
            this, SLOT(StationListError(QString)));
    connect(stationLister.data(), SIGNAL(finished(QStringList)),
            this, SLOT(StationListFinished(QStringList)));

    connect(useServer, SIGNAL(toggled(bool)), hostname, SLOT(setEnabled(bool)));
    connect(useServer, SIGNAL(toggled(bool)), port, SLOT(setEnabled(bool)));
}

void ServerDetailsPage::initializePage() {
    connect(wizard()->button(QWizard::CustomButton1), SIGNAL(clicked()),
            this, SLOT(subpageBack()));
}


void ServerDetailsPage::switchToSubPage(SubPage subPage) {
    wizard()->button(QWizard::BackButton)->setVisible(false);
    wizard()->button(QWizard::CustomButton1)->setVisible(true);
    wizard()->button(QWizard::CustomButton1)->setEnabled(true);

    if (subPage == SL_DetailsPage) {
        setTitle(tr("Server Information"));
        setSubTitle(tr("Enter the connection details for the weather server."));
        wizard()->button(QWizard::BackButton)->setVisible(true);
        wizard()->button(QWizard::CustomButton1)->setVisible(false);
        wizard()->button(QWizard::NextButton)->setEnabled(true);
    } else if (subPage == SL_ProgressPage) {
        setTitle(tr("Checking Weather Server"));
        setSubTitle(tr("The configuration wizard is checking the weather "
                       "server connection details."));
        wizard()->button(QWizard::CustomButton1)->setEnabled(false);
        wizard()->button(QWizard::CustomButton1)->setVisible(true);
        wizard()->button(QWizard::NextButton)->setEnabled(false);
    } else if (subPage == SL_Error) {
        wizard()->button(QWizard::NextButton)->setEnabled(false);
        wizard()->button(QWizard::CustomButton1)->setFocus();
    }

    if (stackedLayout != NULL)
        stackedLayout->setCurrentIndex(subPage);

    currentPage = subPage;
    qApp->processEvents();
}

void ServerDetailsPage::subpageBack() {
    qDebug() << "Subpage back.";
    switchToSubPage(SL_DetailsPage);
}

void ServerDetailsPage::showErrorPage(QString title, QString subtitle, QString message) {

    switchToSubPage(SL_Error);

    setTitle(title);
    setSubTitle(subtitle);

    errorLabel->setText(message);
}

bool ServerDetailsPage::validatePage() {

    if (noServer->isChecked() || validated) {
        validated = false;
        return true;
    } else {
        switchToSubPage(SL_ProgressPage);

        stationLister->fetchStationList(hostname->text(), port->value());

        return false;
    }
}

void ServerDetailsPage::validationComplete() {
    wizard()->button(QWizard::BackButton)->setVisible(true);
    wizard()->button(QWizard::CustomButton1)->setVisible(false);
    wizard()->button(QWizard::NextButton)->setEnabled(true);

    switchToSubPage(SL_DetailsPage);

    setField(SERVER_AVAILABLE, true);
    setField(SERVER_HOSTNAME, hostname->text());
    setField(SERVER_PORT, port->value());

    validated = true;
    wizard()->button(QWizard::NextButton)->setEnabled(true);
    wizard()->button(QWizard::NextButton)->click();
}

void ServerDetailsPage::StationListError(QString message) {
    showErrorPage(tr("Server Error"), tr("An error occurred validating server details"),
                  message);
}

void ServerDetailsPage::StationListFinished(QStringList stations) {

    QString stationCode;

    bool multipleStations = field(MULTIPLE_STATIONS_AVAILABLE_FIELD).toBool();
    if (multipleStations) {
        stationCode = field(SELECTED_STATION_CODE).toString();
    } else {
        DbUtil::StationInfo station = field(FIRST_STATION_FIELD).value<DbUtil::StationInfo>();
        stationCode = station.code;
    }

    if (!stations.contains(stationCode)) {
        showErrorPage(tr("Server Error"),
                      tr("The selected station was not available on the server"),
                      QString(tr("The server specified does not cary data for the <i>%1</i> "
                              "station. Click the <b>Back</b> button and either enter "
                              "details for a different server that does carry data "
                              "for this station or choose the <b>No Server</b> option.")).arg(stationCode));
        return;
    }

    validationComplete();
}

int ServerDetailsPage::nextId() const {
    return ConfigWizard::Page_ConfirmDetails;
}

