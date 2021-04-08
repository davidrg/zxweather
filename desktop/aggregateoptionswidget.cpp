#include "aggregateoptionswidget.h"
#include "ui_aggregateoptionswidget.h"
#include "datasource/aggregate.h"

AggregateOptionsWidget::AggregateOptionsWidget(QWidget *parent) :
    QWidget(parent),
    ui(new Ui::AggregateOptionsWidget)
{
    ui->setupUi(this);
    setRainEvapoOptionsEnabled(true);
}

AggregateOptionsWidget::~AggregateOptionsWidget()
{
    delete ui;
}

AggregateFunction AggregateOptionsWidget::getAggregateFunction() {
    int function = ui->cbMethod->currentIndex();

    switch(function) {
    case 0:
        return AF_Average;
    case 1:
        return AF_Minimum;
    case 2:
        return AF_Maximum;
    case 3:
        return AF_Sum;
    case 4:
        return AF_RunningTotal;
    default:
        return AF_None;
    }
}

AggregateGroupType AggregateOptionsWidget::getAggregateGroupType() {
    if (ui->rbHourly->isChecked())
        return AGT_Hour;
    if (ui->rbDaily->isChecked())
        return AGT_Day;
    if (ui->rbMonthly->isChecked())
        return AGT_Month;
    if (ui->rbYearly->isChecked())
        return AGT_Year;
    return AGT_Custom;
}

uint32_t AggregateOptionsWidget::getCustomMinutes() {
    if (ui->rbWeekly->isChecked())
        return 10080;
    if (ui->rbCustom->isChecked())
        return ui->sbCustomMinutes->value();
    return 0;
}

void AggregateOptionsWidget::setRainEvapoOptionsEnabled(bool enabled) {

    if (enabled == rainEvapoOptionsEnabled) {
        return; // Nothing to do.
    }

    int currentIndex = ui->cbMethod->currentIndex();

    QStringList options;
    options << tr("Average")
            << tr("Minimum")
            << tr("Maximum");

    if (enabled) {
        options << tr("Sum")
                << tr("Running Total");
    }

    rainEvapoOptionsEnabled = enabled;
    ui->cbMethod->clear();
    ui->cbMethod->addItems(options);

    if (currentIndex < ui->cbMethod->count()) {
        ui->cbMethod->setCurrentIndex(currentIndex);
    }
}

bool AggregateOptionsWidget::isRainEvapoOptionsEnabled() {
    return rainEvapoOptionsEnabled;
}
