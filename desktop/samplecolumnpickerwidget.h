#ifndef SAMPLECOLUMNPICKERWIDGET_H
#define SAMPLECOLUMNPICKERWIDGET_H

#include "columnpickerwidget.h"

class SampleColumnPickerWidget : public ColumnPickerWidget
{
    Q_OBJECT

public:
    SampleColumnPickerWidget(QWidget *parent = NULL);


    /** Configures available columns based on the weather station hardware
     * and data source configuration
     *
     * @param solarAvailable If solar sensors are available (implies Vantage Pro2 Plus)
     * @param hw_type If a Davis station is being used
     * @param isWireless If a wireless davis station is being used
     * @param extraColumns Enabled extra columns
     * @param extraColumnNames Names for enabled extra columns
     * @param forecastRule Show the Forecast Rule ID checkbox
     */
    void configure(bool solarAvailable, hardware_type_t hw_type, bool isWireless,
                   ExtraColumns extraColumns,
                   QMap<ExtraColumn, QString> extraColumnNames,
                   bool forecastRule = false);

    /** Used by the add graph dialog. Checks all specified checkboxes and disables
     * them. These columns won't be returned by getNewColumns.
     * @param columns
     */
    void checkAndLockColumns(SampleColumns columns);

    /** Returns all checked columns (including those that have been disabled via
     * checkAndLockColumns(SampleColumns).
     *
     * @return All checked columns
     */
    SampleColumns getColumns() const;

    /** Returns all checked columns that weren't specified in any previous call to
     * checkAndLockColumns(SampleColumns)
     *
     * @return Selected columns
     */
    SampleColumns getNewColumns() const;

private:

    SampleColumns lockedColumns;

};

#endif // SAMPLECOLUMNPICKERWIDGET_H
