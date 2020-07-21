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
    ui->lblConsoleBatteryA->setText("0.0 V");
    ui->lblConsoleBatteryB->setText("");
    ui->lblTxBattery->setText("unknown");
}

void StatusWidget::refreshLiveData(LiveDataSet lds) {
    if (lds.hw_type != HW_DAVIS)
        return; // Only supported on Davis hardware.

    updateCount++;
    ui->lblUpdateCount->setText(QString::number(updateCount));

    QString bat_voltage = QString::number(lds.davisHw.consoleBatteryVoltage, 'f', 2) + " V";
    if (lds.davisHw.consoleBatteryVoltage <= 3.5) {
        // We need two labels to maintain proper vertical alignment. We'd just turn the visibility
        // of the icon label on and off but it seems to still occupy space in the layout when
        // its hidden. So we do this...
        ui->lblConsoleBatteryA->setText("<img src=':/icons/battery-low' />");
        ui->lblConsoleBatteryB->setText(bat_voltage);
    } else {
        ui->lblConsoleBatteryA->setText(bat_voltage);
        ui->lblConsoleBatteryB->setText("");
    }

    // I can't find anything that explains the transmitter  battery status
    // byte but what I can find suggests that it gives the status for
    // transmitters 1-8. I'm assuming it must be a bitmap.
    QString txStatus = tr("bad: ");
    char txStatusByte = (char)lds.davisHw.txBatteryStatus;
    for (int i = 0; i < 8; i++) {
        if (CHECK_BIT(txStatusByte, i))
            txStatus.append(QString::number(i) + ", ");
    }
    if (txStatus == tr("bad: "))
        txStatus = tr("ok");
    else
        txStatus = txStatus.mid(0, txStatus.length() - 2);
    ui->lblTxBattery->setText(txStatus);
}

void StatusWidget::setTransmitterBatteryVisible(bool visible) {
    ui->lblTxBattery->setVisible(visible);
    ui->txBatteryLabel->setVisible(visible);
    updateGeometry();
    adjustSize();
}
