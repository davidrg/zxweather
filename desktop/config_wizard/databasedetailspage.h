#ifndef DATABASEDETAILSPAGE_H
#define DATABASEDETAILSPAGE_H

#include <QWizardPage>
#include <QVariant>
#include <QSqlDatabase>

class QLineEdit;
class QSpinBox;
class QProgressBar;
class QLabel;
class QStackedLayout;

class DatabaseDetailsPage : public QWizardPage {
    Q_OBJECT

public:
    DatabaseDetailsPage(QWidget *parent = 0);

    int nextId() const;

    bool validatePage();

    virtual void initializePage();
    virtual void cleanupPage();

public slots:
    void subpageBack();

private:
    QWidget* detailsPage;
    QLineEdit* databaseName;
    QLineEdit* hostName;
    QSpinBox* port;
    QLineEdit* userName;
    QLineEdit* password;

    QWidget* progressPage;
    QProgressBar* progressBar;
    QLabel* progress;

    QWidget* errorPage;
    QLabel* errorLabel;

    QStackedLayout* stackedLayout;

    typedef enum {
        SL_DetailsPage = 0,
        SL_ProgressPage = 1,
        SL_Error = 2
    } SubPage;

    SubPage currentPage;

    bool multipleStationsPresent;

    void switchToSubPage(SubPage subPage);
    void showErrorPage(QString title, QString subtitle, QString message);

    // validation functions
    bool checkDriver();
    bool connectDb(QSqlDatabase &db);
    bool checkCompatibility(QSqlDatabase db);
};

#endif // DATABASEDETAILSPAGE_H
