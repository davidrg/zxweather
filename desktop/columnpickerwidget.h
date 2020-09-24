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

signals:
    void columnSelectionChanged();


public slots:
    /** Checks all enabled checkboxes
     */
    void checkAll();

    /** Unchecks all enabled checkboxes
     */
    void uncheckAll();

private slots:
    void checkboxToggled(bool checked);

protected:
    Ui::ColumnPickerWidget *ui;

    /** Performs basic UI configuration including disabling any checkboxes not supported
     * by the current hardware and configuring any enabled extra sensor checkboxes.
     *
     * @param solarAvailable If solar sensors are available (implies Vantage Pro2 Plus)
     * @param hw_type If a Davis station is being used
     * @param isWireless If a wireless davis station is being used
     * @param extraColumns Enabled extra columns
     * @param extraColumnNames Names for enabled extra columns
     */
    void configureUi(
            bool solarAvailable, hardware_type_t hw_type, bool isWireless,
            ExtraColumns extraColumns,
            QMap<ExtraColumn, QString> extraColumnNames);

    void hideSolarColumns();
    void hideWirelessReceptionColumn();
    void hideDavisOnlyColumns();
    void configureExtraColumns(ExtraColumns extraColumns,
                               QMap<ExtraColumn, QString> extraColumnNames);

    void focusFirstAvailableTab();

private:
    QMap<int, QString> tabLabels;
};

#endif // COLUMNPICKERWIDGET_H
