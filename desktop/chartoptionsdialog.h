#ifndef CHARTOPTIONSDIALOG_H
#define CHARTOPTIONSDIALOG_H

#include <QDialog>

namespace Ui {
class ChartOptionsDialog;
}

class ChartOptionsDialog : public QDialog
{
    Q_OBJECT
    
public:
    explicit ChartOptionsDialog(QWidget *parent = 0);
    ~ChartOptionsDialog();
    
private:
    Ui::ChartOptionsDialog *ui;
};

#endif // CHARTOPTIONSDIALOG_H
