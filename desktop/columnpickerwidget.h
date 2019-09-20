#ifndef COLUMNPICKERWIDGET_H
#define COLUMNPICKERWIDGET_H

#include <QWidget>

// AbstractLiveDatasource for hardware_type_t
#include "datasource/abstractlivedatasource.h"
#include "datasource/samplecolumns.h"


namespace Ui {
class ColumnPickerWidget;
}

class ColumnPickerWidget : public QWidget
{
    Q_OBJECT

public:
    explicit ColumnPickerWidget(QWidget *parent = NULL);
    ~ColumnPickerWidget();

    void configure(bool solarAvailable, hardware_type_t hw_type, bool isWireless,
                   ExtraColumns extraColumns,
                   QMap<ExtraColumn, QString> extraColumnNames);

    SampleColumns getColumns();

private slots:
    void checkboxToggled(bool checked);

private:
    Ui::ColumnPickerWidget *ui;

    QMap<int, QString> tabLabels;
};

#endif // COLUMNPICKERWIDGET_H
