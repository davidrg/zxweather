#ifndef AXISTYPE_H
#define AXISTYPE_H

/** Different types of axes. With the exception of AT_KEY there will only
 * ever be one of each axis type in the chart
 */
typedef enum {
    AT_NONE = 0, /*!< Not a real axis. */
    AT_TEMPERATURE = 1, /*!< axis in degrees celsius */
    AT_WIND_SPEED = 2, /*!< axis in m/s */
    AT_WIND_DIRECTION = 3, /* Axis for wind direction */
    AT_PRESSURE = 4, /*!< Axis for hPa */
    AT_HUMIDITY = 5, /*!< Axis in % */  /* NOTE: The value (type==5) is used in pluscursor.cpp*/
    AT_RAINFALL = 6, /*!< Axis in mm */
    AT_SOLAR_RADIATION = 7, /*!< Axis in W/m^2 */
    AT_UV_INDEX = 8, /*!< Axis for UV Index - no unit */
    AT_RAIN_RATE = 9, /*!< Axis for Rain rate in mm/h */
    AT_RECEPTION = 10, /*!< Axis for wireless reception (%) */
    AT_EVAPOTRANSPIRATION = 11, /*!< Axis for Evapotrainspiration in mm */
    AT_SOIL_MOISTURE = 12, /*!< Axis for soil moisture in centibar */
    AT_LEAF_WETNESS = 13, /*!< Axis for leaf wetness */
    AT_VOLTAGE = 14, /*!< Axis for console battery voltage (live data) */
    AT_KEY = 100 /*!< X Axis for DataSet 0. AT_KEY+1 for DataSet 1, etc. */
} AxisType;

#endif // AXISTYPE_H
