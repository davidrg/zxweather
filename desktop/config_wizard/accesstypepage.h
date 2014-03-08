#ifndef ACCESSTYPEPAGE_H
#define ACCESSTYPEPAGE_H

#include <QWizardPage>
#include <QVariant>

class QLabel;
class QRadioButton;

class AccessTypePage : public QWizardPage {
    Q_OBJECT

    Q_PROPERTY(bool multipleStationsArePresent READ getMultipleStationsPresent WRITE setMultipleStationsPresent)
    Q_PROPERTY(QVariant stationList READ getStationList WRITE setStationList)
    Q_PROPERTY(QVariant station READ getStation WRITE setStation)

    Q_PROPERTY(bool serverAvailable READ getServerAvailable WRITE setServerAvailable)
    Q_PROPERTY(QString serverHostname READ getServerHostname WRITE setServerHostname)
    Q_PROPERTY(int serverPort READ getServerPort WRITE setServerPort)

public:
    AccessTypePage(QWidget *parent = 0);

    int nextId() const;

    bool isComplete() const;

    // These properties are set by other pages to tell us the available stations
    // to choose from.
    bool getMultipleStationsPresent() const { return multipleStationsPresent; }
    void setMultipleStationsPresent(bool value) { multipleStationsPresent = value; }
    QVariant getStationList() const { return stationList; }
    void setStationList(QVariant value) { stationList = value; }
    QVariant getStation() const { return firstStation; }
    void setStation(QVariant value) { firstStation = value; }

    bool getServerAvailable() { return serverAvailable; }
    void setServerAvailable(bool available) { serverAvailable = available; }
    QString getServerHostname() { return serverHostname; }
    void setServerHostname(QString hostname) { serverHostname = hostname; }
    int getServerPort() { return serverPort; }
    void setServerPort(int port) { serverPort = port; }

private slots:
    void completenessChanged();

private:
    QLabel* topLabel;
    QLabel* optionHeading;
    QRadioButton* rbLocal;
    QRadioButton* rbInternet;

    bool multipleStationsPresent;
    QVariant stationList;
    QVariant firstStation;

    // For internet stations only: Live data source details.
    bool serverAvailable;
    QString serverHostname;
    int serverPort;
};

#endif // ACCESSTYPEPAGE_H
