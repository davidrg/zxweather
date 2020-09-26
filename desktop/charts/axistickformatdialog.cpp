#include "axistickformatdialog.h"
#include "ui_axistickformatdialog.h"

#include "datetimeformathelpdialog.h"
#include "json/json.h"

AxisTickFormatDialog::AxisTickFormatDialog(QWidget *parent) :
    QDialog(parent),
    ui(new Ui::AxisTickFormatDialog)
{
    ui->setupUi(this);
    connect(ui->pbHelp, SIGNAL(clicked(bool)), this, SLOT(showHelp()));
}

AxisTickFormatDialog::~AxisTickFormatDialog()
{
    delete ui;
}

void AxisTickFormatDialog::showHelp() {
    DateTimeFormatHelpDialog *help = new DateTimeFormatHelpDialog();
    help->setAttribute(Qt::WA_DeleteOnClose);
    help->show();
}

void AxisTickFormatDialog::setFormat(key_axis_tick_format_t format, QString customFormat) {
    switch(format) {
    case KATF_Default:
        ui->rbDefault->setChecked(true);
        break;
    case KATF_DefaultNoYear:
        ui->rbDefaultNoYear->setChecked(true);
        break;
    case KATF_Time:
        ui->rbTime->setChecked(true);
        break;
    case KATF_Date:
        ui->rbDate->setChecked(true);
        break;
    case KATF_Custom:
        ui->rbCustom->setChecked(true);

        // Use the JSON parser to handle escape sequences
        customFormat = QtJson::Json::serialize(customFormat);

        // The serialised version will be quoted. So remove the quotes.
        customFormat = customFormat.mid(1,customFormat.length()-2);

        ui->leCustomFormat->setText(customFormat);
        break;
    default:
        ui->rbDefault->setChecked(true);
    }
}

key_axis_tick_format_t AxisTickFormatDialog::getFormat() {
    if (ui->rbDefault->isChecked()) {
        return KATF_Default;
    } else if (ui->rbDefaultNoYear->isChecked()) {
        return KATF_DefaultNoYear;
    } else if (ui->rbTime->isChecked()) {
        return KATF_Time;
    } else if (ui->rbDate->isChecked()) {
        return KATF_Date;
    } else if (ui->rbCustom->isChecked()) {
        return KATF_Custom;
    }

    return KATF_Default;
}

QString AxisTickFormatDialog::getFormatString() {
    if (getFormat() == KATF_Custom) {
        QString fmt = ui->leCustomFormat->text();

        // Use the JSON parser to handle escape sequences
        QVariant parsed = QtJson::Json::parse(QString("\"%1\"").arg(fmt));
        if (parsed.type() == QVariant::String) {
            fmt = parsed.toString();
        }

        return fmt;
    }

    return "";
}
