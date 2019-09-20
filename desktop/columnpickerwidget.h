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

    /** Configures available columns based on the weather station hardware
     * and data source configuration
     *
     * @param solarAvailable If solar sensors are available (implies Vantage Pro2 Plus)
     * @param hw_type If a Davis station is being used
     * @param isWireless If a wireless davis station is being used
     * @param extraColumns Enabled extra columns
     * @param extraColumnNames Names for enabled extra columns
     */
    void configure(bool solarAvailable, hardware_type_t hw_type, bool isWireless,
                   ExtraColumns extraColumns,
                   QMap<ExtraColumn, QString> extraColumnNames);

    /** Used by the add graph dialog. Checks all specified checkboxes and disables
     * them. These columns won't be returned by getNewColumns.
     * @param columns
     */
    void checkAndLockColumns(SampleColumns columns);

    /** Checks all enabled checkboxes
     */
    void checkAll();

    /** Unchecks all enabled checkboxes
     */
    void uncheckAll();

    /** Returns all checked columns (including those that have been disabled via
     * checkAndLockColumns(SampleColumns).
     *
     * @return All checked columns
     */
    SampleColumns getColumns();

    /** Returns all checked columns that weren't specified in any previous call to
     * checkAndLockColumns(SampleColumns)
     *
     * @return Selected columns
     */
    SampleColumns getNewColumns();

private slots:
    void checkboxToggled(bool checked);

private:
    Ui::ColumnPickerWidget *ui;

    QMap<int, QString> tabLabels;

    SampleColumns lockedColumns;
};

#endif // COLUMNPICKERWIDGET_H
