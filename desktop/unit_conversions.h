#ifndef UNIT_CONVERSIONS_H
#define UNIT_CONVERSIONS_H

#include <QString>
#include <stdint.h>

namespace UnitConversions {
    typedef enum _unit_t {
        // Wind speed
        U_METERS_PER_SECOND,
        U_KILOMETERS_PER_HOUR,
        U_MILES_PER_HOUR,
        U_BFT,
        U_KNOTS,

        // Temperature
        U_CELSIUS,
        U_FAHRENHEIT,

        // atmospheric pressure
        U_HECTOPASCALS,
        U_INCHES_OF_MERCURY,
        U_DAVIS_BAROMETER_TREND,

        // Rainfall
        U_MILLIMETERS,
        U_CENTIMETERS,
        U_INCHES,

        // Rain rate
        U_MILLIMETERS_PER_HOUR,
        U_CENTIMETERS_PER_HOUR,
        U_INCHES_PER_HOUR,

        // Solar Radiation
        U_WATTS_PER_SQUARE_METER,

        U_UV_INDEX,

        U_HUMIDITY,

        U_LEAF_WETNESS,

        // Soil Moisture
        U_CENTIBAR,

        // Wind Direction
        U_DEGREES,
        U_COMPASS_POINT,

        U_VOLTAGE,

        U_UNKNOWN
    } unit_t;

    class UnitValue {

    public:
        UnitValue(const UnitValue &other);
        UnitValue(float v);
        UnitValue(double v);
        UnitValue(int v);
        UnitValue();

        unit_t unit;

        operator int() const;
        operator float() const;
        operator QString() const;

    private:
        union {
            float floatValue;
            int32_t intValue;
        } value;
        bool isInt;
    };

    typedef struct _value_t {
        double value;
        int intValue;
        unit_t unit;
    } value_t;

    int metersPerSecondtoBFT(double ms);
    inline double metersPerSecondToKilometersPerHour(double ms) {
        return ms * 3.6;
    }
    inline double metersPerSecondToKnots(double ms) {
        double kmh = metersPerSecondToKilometersPerHour(ms);
        return kmh / 1.852; // 1 knot is exactly 1.852 km/h
    }
    inline double metersPerSecondToMilesPerHour(double ms) {
        return ms * 2.23694;
    }

    UnitValue metersPerSecondToKilometersPerHour(const UnitValue &v);
    UnitValue metersPerSecondToKnots(const UnitValue &v);
    UnitValue toImperial(const UnitValue &v);

    inline double celsiusToFahrenheit(double c) {
        return 1.8 * c + 32;
    }

    inline double hectopascalsToInchesOfMercury(double hpa) {
        return hpa * 0.02953;
    }

    inline double millimetersToCentimeters(double mm) {
        return mm * 0.1;
    }
    inline double millimetersToInches(double mm) {
        return mm * 1.0/25.4;
    }

    QString davisBarometerTrendLabel(int trend);

    QString windDirectionToCompassPoint(int windDirection);
    QString bftToString(int bft);
    QString metersPerSecondToBFTString(double ms);

    unit_t metricToImperial(unit_t unit);

    QString unitString(unit_t unit);
}

#endif // UNIT_CONVERSIONS_H
