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
                            ExtraColumns extraColumns,
                            QMap<ExtraColumn, QString> extraColumnNames,
                            QWidget *parent = 0);
    ~AddGraphDialog();
    
    SampleColumns selectedColumns();

    // Gets the set of columns that this window supports returning via
    // the selectedColumns() method.
    static SampleColumns supportedColumns(hardware_type_t hw_type,
                                          bool isWireless, bool hasSolar,
                                          ExtraColumns extraColumns);

private:
    Ui::AddGraphDialog *ui;
};

#endif // ADDGRAPHDIALOG_H
