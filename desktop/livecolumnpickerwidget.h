#ifndef LIVECOLUMNPICKERWIDGET_H
#define LIVECOLUMNPICKERWIDGET_H

#include "columnpickerwidget.h"
#include "datasource/abstractlivedatasource.h"

/** A version of the column picker for live data feeds. This consists of a slightly
 * different selection of columns from those stored in the database.
 *
 */
class LiveColumnPickerWidget : public ColumnPickerWidget
{
public:
    LiveColumnPickerWidget(QWidget *parent = NULL);

    /** Configures available live data columns based on the weather station hardware
     * and data source configuration.
     *
     * Calling this function:
     *   - Renames Rainfall to Storm Rain
     *   - Renames High Rain Rate to Rain Rate
     *   - Renames (Wind) Average Speed to Wind Speed
     *   - Renames (Wind) Average Direction to Wind Direction
     *   - Hides Evapotranspiration
     *   - Hides Wireless Reception
     *   - Hides Forecast Rule ID
     *   - Shows Console Battery Voltage
     *
     * @param solarAvailable If solar sensors are available (implies Vantage Pro2 Plus)
     * @param hw_type If a Davis station is being used
     * @param extraColumns Enabled extra columns
     * @param extraColumnNames Names for enabled extra columns
     */
    void configure(bool solarAvailable, bool indoorDataAvailable, hardware_type_t hw_type,
                   ExtraColumns extraColumns,
                   QMap<ExtraColumn, QString> extraColumnNames);

    /** Used by the add graph dialog. Checks all specified checkboxes and disables
     * them. These columns won't be returned by getNewColumns.
     * @param columns
     */
    void checkAndLockColumns(LiveValues columns);

    /** Returns all checked columns (including those that have been disabled via
     * checkAndLockColumns(SampleColumns).
     *
     * @return All checked columns
     */
    LiveValues getColumns() const;

    /** Returns all checked columns that weren't specified in any previous call to
     * checkAndLockColumns(SampleColumns)
     *
     * @return Selected columns
     */
    LiveValues getNewColumns() const;

private:

    LiveValues lockedColumns;
};

#endif // LIVECOLUMNPICKERWIDGET_H
