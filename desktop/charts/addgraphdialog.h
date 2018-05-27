#ifndef ADDGRAPHDIALOG_H
#define ADDGRAPHDIALOG_H

#include <QDialog>
#include "datasource/samplecolumns.h"

// For hardware_type_t
#include "datasource/abstractlivedatasource.h"

namespace Ui {
class AddGraphDialog;
}

class AddGraphDialog : public QDialog
{
    Q_OBJECT
    
public:
    explicit AddGraphDialog(SampleColumns availableColumns,
                            bool solarAvailable,
                            bool isWireless,
                            hardware_type_t hw_type,
                            QWidget *parent = 0);
    ~AddGraphDialog();
    
    SampleColumns selectedColumns();

private:
    Ui::AddGraphDialog *ui;
};

#endif // ADDGRAPHDIALOG_H
