#ifndef INTERNETSITEINFOPAGE_H
#define INTERNETSITEINFOPAGE_H

#include <QWizardPage>
#include <QAbstractSocket>
#include <QScopedPointer>
#include <QStringList>

#include "dbutil.h"
#include "serverstationlister.h"

class QNetworkReply;
class QLineEdit;
class QStackedLayout;
class QLabel;
class QNetworkAccessManager;

struct ServerStation {
    QString code;
    QString hostname;
    int port;
    bool available;
};
Q_DECLARE_METATYPE(ServerStation)
Q_DECLARE_METATYPE(QList<ServerStation*>)

#define ISI_DETAIL_SUBTITLE "Enter the base URL for the Web Interface (for example, " \
                            "http://weather.example.com/)"

class InternetSiteInfoPage : public QWizardPage {
    Q_OBJECT

    Q_PROPERTY(QVariant serverStationStatus READ getStationStatus)

public:
    InternetSiteInfoPage(QWidget *parent = 0);
    ~InternetSiteInfoPage();

    int nextId() const;
    void initializePage();
    void cleanupPage();
    bool validatePage();

    QVariant getStationStatus() const;

private slots:
    void subpageBack();
    void requestFinished(QNetworkReply *reply);

    void StationListError(QString message);
    void StationListFinished(QStringList stations);

private:

    // TODO: ensure these are cleaned up properly
    QLineEdit *baseUrlEdit;
    QStackedLayout *stackedLayout;
    QLabel* progress;
    QLabel* errorLabel;
    QNetworkAccessManager* netManager;


    QList<DbUtil::StationInfo> stations;
    QString serverHostname;
    int port;
    bool serverAvailable;
    bool multipleStationsPresent;
    QVariant stationList;
    QVariant firstStation;
    QList<ServerStation*> serverStationAvailability;

    QScopedPointer<ServerStationLister> stationLister;


    typedef enum {
        SL_DetailsPage = 0,
        SL_ProgressPage = 1,
        SL_Error = 2
    } SubPage;
    int currentPage;



    void switchToSubPage(SubPage subPage);
    void showErrorPage(QString title, QString subtitle, QString message);

    void validationComplete();

    void sendNextCommand();

    void processStationList(QString jsonData);

    bool validated;
};

#endif // INTERNETSITEINFOPAGE_H
