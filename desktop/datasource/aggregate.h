#ifndef AGGREGATE_H
#define AGGREGATE_H

/** Aggregate function to use when grouping data.
 */
enum AggregateFunction {
    AF_None,    /*!< Don't group data */
    AF_Average, /*!< Compute average */
    AF_Minimum, /*!< Minimum value within group */
    AF_Maximum, /*!< Maximum value within group */
    AF_Sum,     /*!< Sum of all values in group. Only useful for rainfall. */
    AF_RunningTotal /*!< Sum of all values in column so far. Only useful for rainfall. */
};

/** How data should be grouped. A number of minutes greater than five
 * must be specified along with AGT_Custom.
 */
enum AggregateGroupType {
    AGT_None,   /*!< Don't group data */
    AGT_Month,  /*!< Group data on month */
    AGT_Year,   /*!< Group data on year */
    AGT_Custom  /*!< Group data by minute. */
};

#endif // AGGREGATE_H
