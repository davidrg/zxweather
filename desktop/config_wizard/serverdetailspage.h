#ifndef SERVERDETAILSPAGE_H
#define SERVERDETAILSPAGE_H

#include <QWizardPage>
#include <QLineEdit>
#include <QSpinBox>
#include <QLabel>
#include <QProgressBar>
#include <QScopedPointer>
#include <QRadioButton>
#include <QStackedLayout>

#include "serverstationlister.h"

class ServerDetailsPage : public QWizardPage {
    Q_OBJECT

public:
    ServerDetailsPage(QWidget *parent = 0);

    int nextId() const;

    bool validatePage();

    void initializePage();

public slots:
    void subpageBack();

    void StationListError(QString message);
    void StationListFinished(QStringList stations);

private:
    QStackedLayout *stackedLayout;
    QWidget* detailsPage;
    QRadioButton* noServer;
    QRadioButton* useServer;
    QLineEdit* hostname;
    QSpinBox* port;

    QWidget* progressPage;
    QProgressBar* progressBar;
    QLabel* progress;

    QWidget* errorPage;
    QLabel* errorLabel;

    bool validated;

    typedef enum {
        SL_DetailsPage = 0,
        SL_ProgressPage = 1,
        SL_Error = 2
    } SubPage;

    SubPage currentPage;

    QScopedPointer<ServerStationLister> stationLister;

    void switchToSubPage(SubPage subPage);
    void showErrorPage(QString title, QString subtitle, QString message);

    void validationComplete();
};

#endif // SERVERDETAILSPAGE_H
