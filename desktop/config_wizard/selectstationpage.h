#ifndef SELECTSTATIONPAGE_H
#define SELECTSTATIONPAGE_H

#include <QWizardPage>
#include <QHash>
#include <QVariant>
#include <QDialog>

#include "dbutil.h"

class QLayout;
class QSignalMapper;
class QVBoxLayout;

class SelectStationPage : public QWizardPage {
    Q_OBJECT
    Q_PROPERTY(QString stationCode READ getStationCode)
    Q_PROPERTY(QString stationTitle READ getStationTitle)

public:
    SelectStationPage(QWidget *parent = 0);

    int nextId() const;

    void initializePage();

    bool isComplete() const;

    QString getStationCode() const { return selectedStationCode; }
    QString getStationTitle() const { return selectedStationTitle; }

private slots:
    void stationDetailsClick(QString code);
    void stationRadioButtonClick(QWidget* object);

private:
    QLayout* createStationOption(DbUtil::StationInfo info);

    QHash<QString, DbUtil::StationInfo> stationInfoByCode;

    QSignalMapper* optionMapper;
    QString selectedStationCode;
    QString selectedStationTitle;

    QVBoxLayout *mainLayout;
    QWidget* optionListWidget;

    bool isLocal;
    bool serverAvailable;
};

class StationInfoDialog : public QDialog {
    Q_OBJECT

public:
    StationInfoDialog(DbUtil::StationInfo info, QWidget *parent = 0);
};


#endif // SELECTSTATIONPAGE_H
