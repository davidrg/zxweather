#ifndef CONFIRMDETAILSPAGE_H
#define CONFIRMDETAILSPAGE_H

#include <QWizardPage>
#include <QVariant>
#include <QDebug>

class QTableWidget;

class ConfirmDetailsPage : public QWizardPage {
    Q_OBJECT



public:
    ConfirmDetailsPage(QWidget *parent = 0);

    void initializePage();

    int nextId() const;

private:

    void initialiseForDb();
    void initialiseForInternet();
    void setStationName();

    QTableWidget* table;
};

#endif // CONFIRMDETAILSPAGE_H
