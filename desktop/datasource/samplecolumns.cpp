#include "samplecolumns.h"

bool DataSet::operator==(const DataSet& other)
{
    return (other.columns == columns)
            && (other.startTime == startTime)
            && (other.endTime == endTime)
            && (other.id == id)
            && (other.aggregateFunction == aggregateFunction)
            && (other.groupType == groupType)
            && (other.customGroupMinutes == customGroupMinutes);
}
