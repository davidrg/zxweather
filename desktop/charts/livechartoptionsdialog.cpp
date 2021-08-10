#include "livechartoptionsdialog.h"
#include "ui_livechartoptionsdialog.h"
#include "settings.h"
#include "constants.h"

#include <QtDebug>

LiveChartOptionsDialog::LiveChartOptionsDialog(bool aggregate, int period, bool maxRainRate, bool stormRain,
        bool stormRainEnabled, int rangeMinutes, bool tags, bool multiRect,
        Settings::live_multi_axis_label_type_t multiAxisLabels,
        QWidget *parent) :
    QDialog(parent),
    ui(new Ui::LiveChartOptionsDialog)
{
    ui->setupUi(this);

    ui->cmbMultiAxisLabels->addItem(
                tr("Sensor Type - example: Temperature (" DEGREE_SYMBOL "C)"),
                Settings::LMALT_TYPE);
    ui->cmbMultiAxisLabels->addItem(
                tr("Sensor Name - example: Inside Temperature (" DEGREE_SYMBOL "C)"),
                Settings::LMALT_SENSOR);
    ui->cmbMultiAxisLabels->addItem(
                tr("Units only - example: " DEGREE_SYMBOL "C"),
                Settings::LMALT_UNITS_ONLY);

    ui->cbAverageUpdates->setChecked(aggregate);
    ui->sbPeriod->setValue(period);
    ui->cbMaxRainRate->setChecked(maxRainRate);
    ui->cbStormRain->setChecked(stormRain);
    ui->cbStormRain->setEnabled(stormRainEnabled);
    ui->sbTimespan->setValue(rangeMinutes);
    ui->cbAxisTags->setChecked(tags);
    ui->cbMultiAxisRect->setChecked(multiRect);
    ui->cmbMultiAxisLabels->setCurrentIndex(ui->cmbMultiAxisLabels->findData(multiAxisLabels));

    ui->cmbMultiAxisLabels->setEnabled(multiRect);
    ui->lblMultiAxisLabels->setEnabled(multiRect);

    // Set the maximum timespan to be however large the
    // live buffer is.
    ui->sbTimespan->setMaximum(Settings::getInstance().liveBufferHours() * 60);
}

LiveChartOptionsDialog::~LiveChartOptionsDialog()
{
    delete ui;
}

bool LiveChartOptionsDialog::aggregate() const {
    return ui->cbAverageUpdates->isChecked();
}

int LiveChartOptionsDialog::aggregatePeriod() const {
    return ui->sbPeriod->value();
}

bool LiveChartOptionsDialog::maxRainRate() const {
    return ui->cbMaxRainRate->isChecked();
}

bool LiveChartOptionsDialog::stormRain() const {
    return ui->cbStormRain->isChecked();
}

int LiveChartOptionsDialog::rangeMinutes() const {
    return ui->sbTimespan->value();
}

bool LiveChartOptionsDialog::tagsEnabled() const {
    return ui->cbAxisTags->isChecked();
}

bool LiveChartOptionsDialog::multipleAxisRectsEnabled() const {
    return ui->cbMultiAxisRect->isChecked();
}

Settings::live_multi_axis_label_type_t LiveChartOptionsDialog::multiAxisLabels() const {
#if (QT_VERSION >= QT_VERSION_CHECK(5,2,0))
    qDebug() << "Current text" << ui->cmbMultiAxisLabels->currentText() << "data" << ui->cmbMultiAxisLabels->currentData();
    return (Settings::live_multi_axis_label_type_t)ui->cmbMultiAxisLabels->currentData().toInt();
#else
    qDebug() << "Current text" << ui->cmbMultiAxisLabels->currentText() << "data" << ui->cmbMultiAxisLabels->itemData(ui->cmbMultiAxisLabels->currentIndex());
    return (Settings::live_multi_axis_label_type_t)ui->cmbMultiAxisLabels->itemData(ui->cmbMultiAxisLabels->currentIndex()).toInt();
#endif
}
