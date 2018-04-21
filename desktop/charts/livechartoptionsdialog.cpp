#include "livechartoptionsdialog.h"
#include "ui_livechartoptionsdialog.h"

LiveChartOptionsDialog::LiveChartOptionsDialog(bool aggregate, int period, bool maxRainRate, bool stormRain,
        bool stormRainEnabled, int rangeMinutes, bool tags, bool multiRect,
        QWidget *parent) :
    QDialog(parent),
    ui(new Ui::LiveChartOptionsDialog)
{
    ui->setupUi(this);

    ui->cbAverageUpdates->setChecked(aggregate);
    ui->sbPeriod->setValue(period);
    ui->cbMaxRainRate->setChecked(maxRainRate);
    ui->cbStormRain->setChecked(stormRain);
    ui->cbStormRain->setEnabled(stormRainEnabled);
    ui->sbTimespan->setValue(rangeMinutes);
    ui->cbAxisTags->setChecked(tags);
    ui->cbMultiAxisRect->setChecked(multiRect);
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
