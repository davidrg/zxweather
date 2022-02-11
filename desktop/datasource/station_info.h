#ifndef STATION_INFO_H
#define STATION_INFO_H

#include <QString>
#include <QDateTime>

#include "hardwaretype.h"

typedef struct _station_info_t {
    QString title;
    QString description;
    float latitude;
    float longitude;
    bool coordinatesPresent;
    float altitude;
    bool isWireless;
    bool hasSolarAndUV;
    bool isValid;
    hardware_type_t hardwareType;
    unsigned int apiLevel;  /*!< API level for remote stations */
} station_info_t;

typedef struct _sample_range_t {
    QDateTime start;
    QDateTime end;
    bool isValid;
} sample_range_t;

#endif // STATION_INFO_H
