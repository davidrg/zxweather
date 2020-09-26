#ifndef AXISTICKFORMATDIALOG_H
#define AXISTICKFORMATDIALOG_H

#include <QDialog>
#include "weatherplotter.h"

namespace Ui {
class AxisTickFormatDialog;
}

class AxisTickFormatDialog : public QDialog
{
    Q_OBJECT

public:
    explicit AxisTickFormatDialog(QWidget *parent = nullptr);
    ~AxisTickFormatDialog();

    void setFormat(key_axis_tick_format_t format, QString customFormat);
    key_axis_tick_format_t getFormat();
    QString getFormatString();

private slots:
    void showHelp();

private:
    Ui::AxisTickFormatDialog *ui;
};

#endif // AXISTICKFORMATDIALOG_H
