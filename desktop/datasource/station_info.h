#ifndef STATION_INFO_H
#define STATION_INFO_H

typedef struct _station_info_t {
    QString title;
    QString description;
    float latitude;
    float longitude;
    bool coordinatesPresent;
    float altitude;
    bool isValid;
} station_info_t;

#endif // STATION_INFO_H
