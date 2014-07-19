#include "samplecolumns.h"

bool operator==(const DataSet& lhs, const DataSet& rhs)
{
    return (lhs.columns == rhs.columns)
            && (lhs.startTime == lhs.startTime)
            && (lhs.endTime == rhs.endTime)
            && (lhs.id == rhs.id);
}
