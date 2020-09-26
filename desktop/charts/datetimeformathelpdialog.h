#ifndef DATETIMEFORMATHELPDIALOG_H
#define DATETIMEFORMATHELPDIALOG_H

#include <QDialog>

namespace Ui {
class DateTimeFormatHelpDialog;
}

class DateTimeFormatHelpDialog : public QDialog
{
    Q_OBJECT

public:
    explicit DateTimeFormatHelpDialog(QWidget *parent = nullptr);
    ~DateTimeFormatHelpDialog();

private:
    Ui::DateTimeFormatHelpDialog *ui;
};

#endif // DATETIMEFORMATHELPDIALOG_H
