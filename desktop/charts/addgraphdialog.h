#ifndef ADDGRAPHDIALOG_H
#define ADDGRAPHDIALOG_H

#include <QDialog>
#include "datasource/samplecolumns.h"

namespace Ui {
class AddGraphDialog;
}

class AddGraphDialog : public QDialog
{
    Q_OBJECT
    
public:
    explicit AddGraphDialog(SampleColumns availableColumns,
                            bool solarAvailable,
                            QWidget *parent = 0);
    ~AddGraphDialog();
    
    SampleColumns selectedColumns();

private:
    Ui::AddGraphDialog *ui;
};

#endif // ADDGRAPHDIALOG_H
