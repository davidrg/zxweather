#ifndef ADDLIVEGRAPHDIALOG_H
#define ADDLIVEGRAPHDIALOG_H

#include <QDialog>
#include "datasource/abstractlivedatasource.h"

namespace Ui {
class AddLiveGraphDialog;
}

class AddLiveGraphDialog : public QDialog
{
    Q_OBJECT

public:
    explicit AddLiveGraphDialog(LiveValues availableColumns,
                                bool solarAvailable,
                                hardware_type_t hw_type,
                                QString message = QString(),
                                QWidget *parent = 0);
    ~AddLiveGraphDialog();

    LiveValues selectedColumns();

private:
    Ui::AddLiveGraphDialog *ui;
};

#endif // ADDLIVEGRAPHDIALOG_H