#include "unit_conversions.h"
#include "constants.h"

#include <QStringList>
#include <QObject>


UnitConversions::UnitValue::UnitValue(const UnitValue &other) {
    isInt = other.isInt;
    if (isInt) {
        value.intValue = other.value.intValue;
    } else {
        value.floatValue = other.value.floatValue;
    }
    unit = other.unit;
}

UnitConversions::UnitValue::UnitValue(float v) {
    value.floatValue = v;
    isInt = false;
    unit = U_UNKNOWN;
}

UnitConversions::UnitValue::UnitValue(double v) {
    value.floatValue = v;
    isInt = false;
    unit = U_UNKNOWN;
}

UnitConversions::UnitValue::UnitValue(int32_t v) {
    value.intValue = v;
    isInt = true;
    unit = U_UNKNOWN;
}

UnitConversions::UnitValue::UnitValue() {
    value.intValue = 0;
    isInt = true;
    unit = U_UNKNOWN;
}

UnitConversions::UnitValue::operator int() const {
    return isInt ? value.intValue : (int)value.floatValue;
}

UnitConversions::UnitValue::operator float() const {
    return isInt ? value.intValue : value.floatValue;
}

UnitConversions::UnitValue::operator QString() const {

    if (unit == U_BFT) {
        return bftToString(value.intValue);
    } else if (unit == U_DAVIS_BAROMETER_TREND) {
        return davisBarometerTrendLabel(value.intValue);
    } else if (unit == U_COMPASS_POINT) {
        return windDirectionToCompassPoint(value.floatValue);
    }

    QString val;
    if (isInt) {
        val = QString::number(value.intValue);
    } else {
        val = QString::number(value.floatValue, 'f', 1);
    }

    switch(unit) {
    case U_METERS_PER_SECOND:
        return val + " m/s";
    case U_KILOMETERS_PER_HOUR:
        return val + " km/h";
    case U_MILES_PER_HOUR:
        return val + " mph";
    case U_CELSIUS:
        return val + DEGREE_SYMBOL + "C";
    case U_FAHRENHEIT:
        return val + DEGREE_SYMBOL + "F";
    case U_HECTOPASCALS:
        return val + " hPa";
    case U_INCHES_OF_MERCURY:
        return val + " inHg";
    case U_MILLIMETERS:
        return val + " mm";
    case U_CENTIMETERS:
        return val + " cm";
    case U_INCHES:
        return val + " in";
    case U_MILLIMETERS_PER_HOUR:
        return val + " mm/h";
    case U_CENTIMETERS_PER_HOUR:
        return val + " cm/h";
    case U_INCHES_PER_HOUR:
        return val + " in/h";
    case U_WATTS_PER_SQUARE_METER:
        return val + " W/m" SQUARED_SYMBOL;
    case U_UV_INDEX:
        return val;
    case U_HUMIDITY:
        return val + "%";
    case U_DEGREES:
        return val + DEGREE_SYMBOL;
    case U_VOLTAGE:
        return val + "V";
    case U_UNKNOWN:
    default:
        return val + " --unknown--";
    }
}

int UnitConversions::metersPerSecondtoBFT(double ms) {
    if (ms < 0.3)
        return 0;
    else if (ms < 2)
        return 1;
    else if (ms < 3)
        return 2;
    else if (ms < 5.4)
        return 3;
    else if (ms < 8)
        return 4;
    else if (ms < 10.7)
        return 5;
    else if (ms < 13.8)
        return 6;
    else if (ms < 17.1)
        return 7;
    else if (ms < 20.6)
        return 8;
    else if (ms < 24.4)
        return 9;
    else if (ms < 28.3)
        return 10;
    else if (ms < 32.5)
        return 11;
    else
        return 12;
    return -1;
}

QString UnitConversions::bftToString(int bft) {
    switch (bft) {
    case 0:
        return QObject::tr("calm");
    case 1:
        return QObject::tr("light air");
    case 2:
        return QObject::tr("light breeze");
    case 3:
        return QObject::tr("gentle breeze");
    case 4:
        return QObject::tr("moderate breeze");
    case 5:
        return QObject::tr("fresh breeze");
    case 6:
        return QObject::tr("strong breeze");
    case 7:
        return QObject::tr("high wind, near gale");
    case 8:
        return QObject::tr("gale, fresh gale");
    case 9:
        return QObject::tr("strong gale");
    case 10:
        return QObject::tr("storm, whole gale");
    case 11:
        return QObject::tr("violent storm");
    case 12:
        return QObject::tr("hurricane");
    default:
        return "";
    }
}

QString UnitConversions::metersPerSecondToBFTString(double ms) {
    return bftToString(metersPerSecondtoBFT(ms));
}

QString UnitConversions::windDirectionToCompassPoint(int windDirection) {
    QStringList windDirections;
    windDirections << "N" << "NNE" << "NE" << "ENE" << "E" << "ESE" << "SE"
                   << "SSE" << "S" << "SSW" << "SW" << "WSW" << "W" << "WNW"
                   << "NW" << "NNW";
    int idx = (((windDirection * 100) + 1125) % 36000) / 2250;
    return windDirections.at(idx);
}

QString UnitConversions::davisBarometerTrendLabel(int trend) {
    switch (trend) {
    case -60:
        return QObject::tr("falling rapidly");
    case -20:
        return QObject::tr("falling slowly");
    case 0:
        return QObject::tr("steady");
    case 20:
        return QObject::tr("rising slowly");
    case 60:
        return QObject::tr("rising rapidly");
    default:
        return "";
    }
}

double UnitConversions::metersPerSecondToKilometersPerHour(double ms) {
    return ms * 3.6;
}

double UnitConversions::metersPerSecondToMilesPerHour(double ms) {
    return ms * 2.23694;
}

double UnitConversions::celsiusToFahrenheit(double c) {
    return 1.8 * c + 32;
}

double UnitConversions::hectopascalsToInchesOfMercury(double hpa) {
    return hpa * 0.02953;
}


double UnitConversions::millimetersToInches(double mm) {
    return mm * 1.0/25.4;
}

double UnitConversions::millimetersToCentimeters(double mm) {
    return mm * 0.1;
}

UnitConversions::UnitValue UnitConversions::metersPerSecondToKilometersPerHour(const UnitValue &v) {
    if (v.unit != UnitConversions::U_METERS_PER_SECOND) {
        return v;
    }

    UnitConversions::UnitValue kmh = UnitConversions::metersPerSecondToKilometersPerHour((double)v);
    kmh.unit = UnitConversions::U_KILOMETERS_PER_HOUR;

    return kmh;
}

UnitConversions::UnitValue UnitConversions::toImperial(const UnitValue &v) {
    switch(v.unit) {
    case UnitConversions::U_METERS_PER_SECOND: {
        UnitConversions::UnitValue result = UnitConversions::metersPerSecondToMilesPerHour(v);
        result.unit = UnitConversions::U_MILES_PER_HOUR;
        return result;
    }
    case UnitConversions::U_CELSIUS: {
        UnitConversions::UnitValue result = UnitConversions::celsiusToFahrenheit(v);
        result.unit = UnitConversions::U_FAHRENHEIT;
        return result;
    }
    case UnitConversions::U_HECTOPASCALS: {
        UnitConversions::UnitValue result = UnitConversions::hectopascalsToInchesOfMercury(v);
        result.unit = UnitConversions::U_INCHES_OF_MERCURY;
        return result;
    }
    case UnitConversions::U_MILLIMETERS: {
        UnitConversions::UnitValue result = UnitConversions::millimetersToInches(v);
        result.unit = UnitConversions::U_INCHES;
        return result;
    }
    case UnitConversions::U_MILLIMETERS_PER_HOUR: {
        UnitConversions::UnitValue result = UnitConversions::millimetersToInches(v);
        result.unit = UnitConversions::U_INCHES_PER_HOUR;
        return result;
    }
    default:
        return v;
    }
}
