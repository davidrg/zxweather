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
        UnitValue() : UnitValue(0) {}

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
    double metersPerSecondToKilometersPerHour(double ms);
    double metersPerSecondToMilesPerHour(double ms);

    UnitValue metersPerSecondToKilometersPerHour(const UnitValue &v);
    UnitValue toImperial(const UnitValue &v);

    double celsiusToFahrenheit(double c);

    double hectopascalsToInchesOfMercury(double hpa);

    double millimetersToCentimeters(double mm);
    double millimetersToInches(double mm);

    QString davisBarometerTrendLabel(int trend);

    QString windDirectionToCompassPoint(int windDirection);
    QString bftToString(int bft);
    QString metersPerSecondToBFTString(double ms);
}

#endif // UNIT_CONVERSIONS_H
