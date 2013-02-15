#include "statuswidget.h"
#include "ui_statuswidget.h"

#define CHECK_BIT(byte, bit) (((byte >> bit) & 0x01) == 1)

StatusWidget::StatusWidget(QWidget *parent) :
    QWidget(parent),
    ui(new Ui::StatusWidget)
{
    ui->setupUi(this);
    reset();
}

StatusWidget::~StatusWidget()
{
    delete ui;
}

void StatusWidget::reset() {
    updateCount = 0;
    ui->lblUpdateCount->setText("0");
    ui->lblConsoleBattery->setText("0.0 V");
    ui->lblTxBattery->setText("unknown");
}

void StatusWidget::refreshLiveData(LiveDataSet lds) {
    if (lds.hw_type != HW_DAVIS)
        return; // Only supported on Davis hardware.

    updateCount++;
    ui->lblUpdateCount->setText(QString::number(updateCount));

    ui->lblConsoleBattery->setText(
                QString::number(lds.davisHw.consoleBatteryVoltage,
                                'f', 2) + " V");

    // I can't find anything that explains the transmitter battery status
    // byte but what I can find suggests that it gives the status for
    // transmitters 1-8. I'm assuming it must be a bitmap.
    QString txStatus = "bad: ";
    char txStatusByte = (char)lds.davisHw.txBatteryStatus;
    for (int i = 0; i < 8; i++) {
        if (CHECK_BIT(txStatusByte, i))
            txStatus.append(QString::number(i) + ", ");
    }
    if (txStatus == "bad: ")
        txStatus = "ok";
    else
        txStatus = txStatus.mid(0, txStatus.length() - 2);
    ui->lblTxBattery->setText(txStatus);
}
