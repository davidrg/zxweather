#ifndef REQUEST_DATA_H
#define REQUEST_DATA_H

typedef struct _request_data_t {
    SampleColumns columns;
    QDateTime startTime;
    QDateTime endTime;
    AggregateFunction aggregateFunction;
    AggregateGroupType groupType;
    uint32_t groupMinutes;
    QString stationName;
    bool isSolarAvailable;
    hardware_type_t hwType;
} request_data_t;

#endif // REQUEST_DATA_H

