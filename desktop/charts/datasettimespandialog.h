#ifndef DATASETTIMESPANDIALOG_H
#define DATASETTIMESPANDIALOG_H

#include <QDialog>
#include <QDateTime>

namespace Ui {
class DataSetTimespanDialog;
}

class DataSetTimespanDialog : public QDialog
{
    Q_OBJECT

public:
    explicit DataSetTimespanDialog(QWidget *parent = 0);
    ~DataSetTimespanDialog();

    QDateTime getStartTime();
    QDateTime getEndTime();

    void setTime(QDateTime start, QDateTime end);

private:
    Ui::DataSetTimespanDialog *ui;
};

#endif // DATASETTIMESPANDIALOG_H
