#include "samplecolumns.h"

bool DataSet::operator==(const DataSet& other)
{
    return (other.columns.standard == columns.standard)
            && (other.columns.extra == columns.extra)
            && (other.startTime == startTime)
            && (other.endTime == endTime)
            && (other.id == id)
            && (other.aggregateFunction == aggregateFunction)
            && (other.groupType == groupType)
            && (other.customGroupMinutes == customGroupMinutes);
}
