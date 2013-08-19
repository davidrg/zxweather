#ifndef SAMPLECOLUMNS_H
#define SAMPLECOLUMNS_H

#include <QFlags>

enum SampleColumn {
    SC_NoColumns           = 0x0000,
    SC_Timestamp           = 0x0001,
    SC_Temperature         = 0x0002,
    SC_IndoorTemperature   = 0x0004,
    SC_ApparentTemperature = 0x0008,
    SC_WindChill           = 0x0010,
    SC_DewPoint            = 0x0020,
    SC_Humidity            = 0x0040,
    SC_IndoorHumidity      = 0x0080,
    SC_Pressure            = 0x0100,
    SC_Rainfall            = 0x0200,
    SC_AverageWindSpeed    = 0x0400,
    SC_GustWindSpeed       = 0x0800,
    SC_WindDirection       = 0x1000
};
Q_DECLARE_FLAGS(SampleColumns, SampleColumn)

Q_DECLARE_OPERATORS_FOR_FLAGS(SampleColumns)

#define ALL_SAMPLE_COLUMNS (SC_Temperature | SC_IndoorTemperature | \
    SC_ApparentTemperature | SC_WindChill | SC_DewPoint | SC_Humidity | \
    SC_IndoorHumidity | SC_Pressure | SC_Rainfall | SC_AverageWindSpeed | \
    SC_GustWindSpeed | SC_WindDirection | SC_Timestamp)

#define TEMPERATURE_COLUMNS (SC_Temperature | SC_IndoorTemperature | \
    SC_ApparentTemperature | SC_WindChill | SC_DewPoint)

#define HUMIDITY_COLUMNS (SC_Humidity | SC_IndoorHumidity)

#define WIND_COLUMNS (SC_WindDirection | SC_AverageWindSpeed | \
    SC_GustWindSpeed)

#define OTHER_COLUMNS (SC_Pressure | SC_Rainfall)

#endif // SAMPLECOLUMNS_H
