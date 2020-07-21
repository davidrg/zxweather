#include "databasedetailspage.h"
#include "configwizard_private.h"
#include "constants.h"
#include "dbutil.h"

// UI stuff
#include <QLineEdit>
#include <QSpinBox>
#include <QProgressBar>
#include <QLabel>
#include <QStackedLayout>
#include <QFormLayout>
#include <QAbstractButton>

// Misc stuff
#include <QApplication>
#include <QtDebug>

/****************************************************************************
**************************** DATABASE DETAILS PAGE **************************
*****************************************************************************
* > Intro > Access Type [LOCAL] > Database Details
*
* Gathers database connection details (database server and credentials,
* database name).
*
* This page overrides the {Next} button to take the user to a subpage where
* it tries to perform some validation (connects to the server to verify the
* details are correct and tries to get the details of the weather station).
*
****************************************************************************/

DatabaseDetailsPage::DatabaseDetailsPage(QWidget *parent)
    :QWizardPage(parent)
{
    stackedLayout = 0;

    databaseName = new QLineEdit;
    hostName = new QLineEdit;
    port = new QSpinBox;
    port->setMaximum(65535);
    port->setValue(5432);
    userName = new QLineEdit;
    password = new QLineEdit;
    password->setEchoMode(QLineEdit::PasswordEchoOnEdit);

    registerField(DATABASE_FIELD "*", databaseName);
    registerField(DATABASE_HOSTNAME_FIELD "*", hostName);
    registerField(DATABASE_PORT_FIELD, port);
    registerField(DATABASE_USERNAME_FIELD "*", userName);
    registerField(DATABASE_PASSWORD_FIELD "*", password);

    QFormLayout* detailsPageLayout = new QFormLayout;
    detailsPageLayout->addRow(tr("&Database Name:"), databaseName);
    detailsPageLayout->addRow(tr("Server &Host Name:"), hostName);
    detailsPageLayout->addRow(tr("Server P&ort:"), port);
    detailsPageLayout->addRow(tr("&Username:"), userName);
    detailsPageLayout->addRow(tr("&Password:"), password);

    detailsPage = new QWidget;
    detailsPage->setLayout(detailsPageLayout);

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
    setTitle(tr("Database Information"));
    setSubTitle(tr("Enter the connection details for your weather database."));
}

void DatabaseDetailsPage::initializePage() {
    qDebug() << "Database details page init";
    connect(wizard()->button(QWizard::CustomButton1), SIGNAL(clicked()),
            this, SLOT(subpageBack()));
}

void DatabaseDetailsPage::cleanupPage() {
    qDebug() << "Database details page cleanup";
    QWizardPage::cleanupPage();
    disconnect(this, SLOT(subpageBack()));
}

int DatabaseDetailsPage::nextId() const {
    if (multipleStationsPresent)
        return ConfigWizard::Page_SelectStation;
    return ConfigWizard::Page_ConfirmDetails;
}

void DatabaseDetailsPage::switchToSubPage(SubPage subPage) {
    wizard()->button(QWizard::BackButton)->setVisible(false);
    wizard()->button(QWizard::CustomButton1)->setVisible(true);
    wizard()->button(QWizard::CustomButton1)->setEnabled(true);

    if (subPage == SL_DetailsPage) {
        setTitle(tr("Database Information"));
        setSubTitle(tr("Enter the connection details for your weather database."));
        wizard()->button(QWizard::BackButton)->setVisible(true);
        wizard()->button(QWizard::CustomButton1)->setVisible(false);
        wizard()->button(QWizard::NextButton)->setEnabled(true);
    } else if (subPage == SL_ProgressPage) {
        setTitle(tr("Checking Database Connection"));
        setSubTitle(tr("The configuration wizard is checking your database "
                       "connection details and obtaining a list of available "
                       "weather stations."));
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

void DatabaseDetailsPage::subpageBack() {
    qDebug() << "Subpage back.";
    switchToSubPage(SL_DetailsPage);
}

void DatabaseDetailsPage::showErrorPage(QString title, QString subtitle, QString message) {

    switchToSubPage(SL_Error);

    setTitle(title);
    setSubTitle(subtitle);

    errorLabel->setText(message);
}

// Does this ever happen now? Or does disabling the Database option on the access type page
// prevent the application from ever getting here?
bool DatabaseDetailsPage::checkDriver() {
    progress->setText(tr("Check driver"));
    if (!QSqlDatabase::drivers().contains("QPSQL")) {
        QString subtitle = tr("Database Driver Not Found");
#if defined(Q_OS_WIN) || defined(Q_OS_MAC) || defined(Q_OS_OS2) \
    || defined(Q_OS_WINCE) || defined(Q_OS_WIN32) || defined(Q_OS_HAIKU) \
    || defined(Q_OS_MSDOS)
        /* On these platforms we'll probably be running as a binary compiled by
         * someone else using a copy of Qt packaged with the program. No point
         * mentioning Qt at all because there is nothing much the user can do
         * to fix it aside from re-downloading the program.
         */
        QString msg = tr("Your copy of the zxweather desktop client is "
                 "missing the PostgreSQL database driver. You won't be able "
                 "to connect to a weather database without obtaining another "
                 "copy of this program that includes the database driver.");
#else
        /* on UNIX, etc, the version of Qt being used probably came from a
         * package manager of some form. Or the user compiled it themselves.
         * Either way the solution is to either do some more compiling or do
         * some more package-installing (re-downloading this program won't
         * solve anything). So we tell the user exactly whats wrong and some
         * possible solutions.
         */
        QString msg = tr("The version of Qt installed on your system "
                 "does not include the PostgreSQL database driver (QPSQL). "
                 "You will need to install this driver to be able to connect "
                 "to a weather database. Check your systems package manager to"
                 " see if the driver is available or try compiling the driver"
                 " from source code.");
#endif
        showErrorPage(tr("Error"), subtitle, msg);
        return false;
    }
    return true;
}

bool DatabaseDetailsPage::connectDb(QSqlDatabase &db) {
    // Required database drivers exist. Try connecting.
    progress->setText(tr("Connect..."));
    db = QSqlDatabase::addDatabase("QPSQL", DB_NAME);
    db.setHostName(field(DATABASE_HOSTNAME_FIELD).toString());
    db.setPort(field(DATABASE_PORT_FIELD).toInt());
    db.setDatabaseName(field(DATABASE_FIELD).toString());
    db.setUserName(field(DATABASE_USERNAME_FIELD).toString());
    db.setPassword(field(DATABASE_PASSWORD_FIELD).toString());

    bool ok = db.open();

    if (!ok) {
        showErrorPage(tr("Error"), tr("Database Connection Failed"),
                  tr("Connecting to the database server failed. "
                     "Click back to adjust your connection settings and try "
                     "again."));
        return false;
    }
    return true;
}

bool DatabaseDetailsPage::checkCompatibility(QSqlDatabase db) {
    // Database connected ok. Make sure its compatible.
    progress->setText(tr("Check compatibility..."));
    DbUtil::DatabaseCompatibility compatibility =
            DbUtil::checkDatabaseCompatibility(db);
    if (compatibility == DbUtil::DC_BadSchemaVersion
            || compatibility == DbUtil::DC_Incompatible) {

        if (compatibility == DbUtil::DC_Incompatible) {
            // Its an old weather database version thats not compatbile with
            // this version.
            QString minimumVersion = DbUtil::getMinimumAppVersion(db);

            if (!minimumVersion.isNull()) {
                minimumVersion = QString(
                            tr(" You must obtain at version %1 or newer of the "
                               "desktop client to connect to this database."))
                        .arg(minimumVersion);
            }

            showErrorPage(tr("Error"), tr("Incompatible Database"),
                      QString(tr("The database you specified is "
                         "incompatible with this version of the zxweather "
                         "desktop client.%1 Click "
                         "<b>Back</b> to connect to another database or "
                         "<b>Cancel</b> to exit this wizard.")).arg(minimumVersion));
        } else {
            // Database is corrupt or its not a real weather database.
            showErrorPage(tr("Error"), tr("Bad Schema Version"),
                      tr("The database does not look like a zxweather "
                         "database. Click <b>Back</b> to review your "
                         "connection settings or click <b>Cancel</b> to exit "
                         "this wizard."));
        }
        return false;
    } else if (compatibility == DbUtil::DC_Unknown) {
        /* TODO
         * Warning - compatibility check failed.
         */
    }

    return true;
}

bool DatabaseDetailsPage::validatePage() {
    switchToSubPage(SL_ProgressPage);

    if (!checkDriver())
        return false;

    QSqlDatabase db;

    if (!connectDb(db)) {
        QSqlDatabase::removeDatabase(DB_NAME);
        return false;
    }

    if (!checkCompatibility(db)) {
        QSqlDatabase::removeDatabase(DB_NAME);
        return false;
    }

    // Database is probably compatible. Go see how many stations there are.
    progress->setText(tr("Getting station list..."));
    QList<DbUtil::StationInfo> stations = DbUtil::getStationList(db);

    if (stations.isEmpty()) {
        /* TODO
         * Error - something went wrong or there are no stations setup in the
         * database.
         */
        showErrorPage(tr("Error"), tr("No weather stations configured."),
                  QString(tr("There are no weather stations configured in this weather"
                             " database. Consult the zxweather Installation Reference "
                             "manual (%1) for database setup instructions. Click "
                             "<b>Back</b> to connect to another database or "
                             "<b>Cancel</b> to exit this wizard."))
                      .arg(Constants::INSTALLATION_REFERENCE_MANUAL));
        QSqlDatabase::removeDatabase(DB_NAME);
        return false;
    }

    multipleStationsPresent = stations.count() > 1;

    // Pass the list of available stations through to the ConfirmDetailsPage.
    setField(MULTIPLE_STATIONS_AVAILABLE_FIELD, multipleStationsPresent);
    setField(STATION_LIST_FIELD, QVariant::fromValue(stations));
    setField(FIRST_STATION_FIELD, QVariant::fromValue(stations.first()));

    // Done.
    QSqlDatabase::removeDatabase(DB_NAME);
    wizard()->button(QWizard::BackButton)->setVisible(true);
    wizard()->button(QWizard::CustomButton1)->setVisible(false);
    wizard()->button(QWizard::NextButton)->setEnabled(true);

    switchToSubPage(SL_DetailsPage);

    return true;
}
