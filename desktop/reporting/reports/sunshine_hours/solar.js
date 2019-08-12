/****************************************************************************
 * Bright Sunshine Hours: astronomical algorithms                           *
 ****************************************************************************
 * This script handles calculation of the suns position in the sky, the     *
 * earths distance from the sun as well as the two supported maximum solar  *
 * radiation algorithms.                                                    *
 ****************************************************************************/


// Some handy short-hands. Most just call the appropriate Math function. There are a few
// _d versions which take degrees rather than radians to save having to convert each time.
function degrees(angle) {
    return angle * (180.0 / Math.PI);
}

function radians(angle) {
    return angle * (Math.PI / 180.0);
}

function cos(angle_rad) {
    return Math.cos(angle_rad);
}

function cos_d(angle_deg) {
    return cos(radians(angle_deg));
}

function sin(angle_rad) {
    return Math.sin(angle_rad);
}

function sin_d(angle_deg) {
    return sin(radians(angle_deg));
}

function asin(angle) {
    return Math.asin(angle);
}

function tan(angle_rad) {
    return Math.tan(angle_rad);
}

function tan_d(angle_deg) {
    return tan(radians(angle_deg));
}

function atan(angle) {
    return Math.atan(angle);
}


/** Calculates the position of the sun in the sky for a given latitude, longitude and time.
 * 
 * Most formula from http://www.astro.uio.no/~bgranslo/aares/calculate.html
 * 
 * @param at_time time to calculate the suns position for
 * @param latitude observers latitude
 * @param longitude observers longitude
 * @returns A an object containing: right_ascension and declination, altitude and azimuth,
 *          altitude corrected for atmospheric refraction (all in degrees) plus the suns
 *          distance from earth in AU.
 */
function solar_position(at_time, latitude, longitude) {
    
    // The date
    var Y = at_time.year();
    var M = at_time.month() + 1; // stupid january = 0
    var D = at_time.date();
    
    var hours = at_time.hour() + (at_time.minute()/60) + (at_time.second() / 60 / 60);
    
    // Number of days and centuries from J2000.0 epoch
    // ------------------------------------------------
    // This is the number of days from the reference time of 1 January 2000, 12h universal time (J2000.0)
    // This formula is valid from 1-MAR-1900 to 28-FEB-2100.
    
    var epoch_days = 367*Y 
                     - Math.floor((7.0/4.0)*(Y+Math.floor((M+9.0)/12.0))) 
                     + Math.floor(275.0*M/9.0) 
                     + D -730531.5;
    
    var j_centuries = epoch_days / 36525.0;
    
    // Sidereal Time
    // -------------
    // Sidereal time for the Greenwich meridian at midnight universal time
    var tropical_year_length_days = 365.2422;
    var sidereal_time_hours = 6.6974 + 2400.0513 * j_centuries; // in hours
    
    // Sidereal time for the Greenwich meridian at the universal time (hours)
    var sidereal_time_utc = sidereal_time_hours + (366.2422 / tropical_year_length_days) * hours;
    
    var local_sidereal_time = sidereal_time_utc*15 + longitude;
    
    // Solar Coordinates
    // -----------------
    // First, the number of days from J2000.0 for the specified time
    
    var days_from_epoch = epoch_days + hours / 24.0;
    
    var centuries_from_epoch = days_from_epoch / 36525.0;
    
    var sun_mean_longitude_deg = 280.466 + 36000.770 * centuries_from_epoch;
    
    var sun_mean_anomaly_deg = 357.52911 + 35999.05029 * centuries_from_epoch;
    
    // The suns equation of center accounts for the eccentricity of earths orbit around the sun.
    
    var sun_equation_of_center_deg = (1.915 - 0.005 * centuries_from_epoch) * sin_d(sun_mean_anomaly_deg) 
        + 0.020 * sin_d(2 * sun_mean_anomaly_deg);
    
    var eccentricity_of_earth_orbit = 0.016708634 - centuries_from_epoch * (0.000042037 + 0.0000001267 * centuries_from_epoch);

    var sun_true_anomaly = sun_mean_anomaly_deg + sun_equation_of_center_deg;
    
    // Calculate the suns distance in AU
    var sun_distance = (1.000001018 * (1 - eccentricity_of_earth_orbit  * eccentricity_of_earth_orbit )) 
        / (1 + eccentricity_of_earth_orbit  * cos_d(sun_true_anomaly));

    var sun_true_elliptical_longitude_deg = sun_mean_longitude_deg + sun_equation_of_center_deg;
    
    var obliquity_of_ecliptic_deg = 23.439 - 0.013 * centuries_from_epoch;
    
    // The suns right ascension is now:
    //   tan(Rs) = tan(sun_true_elliptical_longitude_deg) * cos(obliquity_of_ecliptic_deg)
    // note that the right ascension is in the same quadrant as the suns true elliptical_longitude
    var sun_ra_rad = Math.atan2(cos_d(obliquity_of_ecliptic_deg) * sin_d(sun_true_elliptical_longitude_deg),
                                cos_d(sun_true_elliptical_longitude_deg));
    
    var sin_sun_decl_rad = sin(sun_ra_rad) * sin_d(obliquity_of_ecliptic_deg);
    var sun_decl_rad = asin(sin_sun_decl_rad);
    
    // Horizontal Coordinates
    // ----------------------
    
    var hour_angle_rad = radians(local_sidereal_time) - sun_ra_rad;

    if (hour_angle_rad > Math.PI) {
        hour_angle_rad -= 2 * Math.PI;
    }
    
    var sin_angular_elevation_rad = sin_d(latitude) * sin(sun_decl_rad) 
        + cos_d(latitude) * cos(sun_decl_rad) * cos(hour_angle_rad); 
    
    // angular elevation in degrees
    var altitude_rad = asin(sin_angular_elevation_rad);
    var altitude_deg = degrees(altitude_rad);
    
    // azimuth measured eastward from the north
    var azimuth_nominator = -sin(hour_angle_rad);
    var azimuth_denominator = (tan(sun_decl_rad) * cos_d(latitude) - sin_d(latitude) * cos(hour_angle_rad));
    var azimuth_rad = atan(azimuth_nominator / azimuth_denominator);
    
    if (azimuth_denominator < 0) { // 2nd or 3rd quadrant
        azimuth_rad += Math.PI;
    } else if (azimuth_nominator < 0) { // 4th quadrant
        azimuth_rad += 2 * Math.PI;
    }
    
    // Atmospheric Refraction
    // ----------------------
    // Compute a corrected altitude that accounts for atmospheric refraction
    // Formula from here: https://www.esrl.noaa.gov/gmd/grad/solcalc/calcdetails.html
    
    var altitude_correction_deg = 0;
    if (altitude_deg <= -0.575) {
        altitude_correction_deg = (-20.774 / tan_d(altitude_deg)) / 3600.0;
    } else if (altitude_deg > -0.575 && altitude_deg <= 5.0) {
        altitude_correction_deg = 
            (1735.0 
            - (518.2*altitude_deg) 
            + (103.4 * Math.pow(altitude_deg,2))
            - (12.79 * Math.pow(altitude_deg, 3))
            + 0.711 * Math.pow(altitude_deg, 4)) / 3600.0
    } else if (altitude_deg > 5.0 && altitude_deg <= 85.0) {
        var tan_alt = tan_d(altitude_deg);
        altitude_correction_deg = (
            58.1 / tan_alt 
            - 0.07 / Math.pow(tan_alt, 3) 
            + 0.000086 / Math.pow(tan_alt, 5)
        ) / 3600.0;
    } // else (altitude > 85.0 && altitude <= 90.0) : no correction needed
    
    // All values are returned in degrees.
    return {
        right_ascension: degrees(sun_ra_rad),
        declination: degrees(sun_decl_rad),
        azimuth: degrees(azimuth_rad),
        altitude: altitude_deg,
        corrected_altitude: altitude_deg + altitude_correction_deg,
        distance: sun_distance // in AU
    };
}

/** Solar Radiation Calculation using the Rafael L Bras algorithm.
 * Bras, R.L.  1990.  Hydrology.  Addison-Wesley, Reading, MA.
 * 
 * Algorithm available in XLS form from the Washing State Department of Ecology
 * SolRad: http://www.ecy.wa.gov/programs/eap/models.html
 * 
 * @param solar_elevation Solar elevation in degrees from the horizon
 * @param sun_distance_au Distance from the sun to the earth in AU
 * @param turbidity Atmospheric turbidity (2=clear, 4-5=smoggy)
 */
function bras_solar(solar_elevation, sun_distance_au, turbidity) {
    var solar_constant = 1367; // in W/m^2
    
    if (sin_d(solar_elevation) < 0) {
        return 0.0; // Sun below horizon. No solar radiation.
    }
    
    // Solar radiation on the horizontal surface at the top of the atmosphere. (EQ 2.9)
    var sol_rad_at_horizontal_surface_top_of_atmosphere = 
        (solar_constant / Math.pow(sun_distance_au, 2)) * sin_d(solar_elevation);
    
    // Optical Air Mass (EQ 2.22)
    var optical_air_mass = Math.pow((sin_d(solar_elevation) + 0.15 * Math.pow(solar_elevation + 3.885, -1.253)) , -1);
    
    // Molecular Scattering Coefficient (EQ 2.26)
    var molecular_scattering_coefficient = 
        0.128 - 0.054 * Math.log(optical_air_mass) / Math.log(10);
    
    // Result: clear-sky solar radiation on a horizontal surface at the earths surface in
    // W/m^2 (EQ 2.25)
    return sol_rad_at_horizontal_surface_top_of_atmosphere * 
        Math.exp(-turbidity * molecular_scattering_coefficient * optical_air_mass);
}

/** Solar Radiation calculation using the Ryan-Stolzenbach algorithm.
 * Ryan, P.J. and K.D. Stolzenbach.  1972.  Engineering aspects of heat disposal from power 
 * generation, (D.R.F. Harleman, ed.).  R.M. Parson Laboratory for Water Resources and 
 * Hydrodynamics, Department of Civil Engineering, Massachusetts Institute of Technology, 
 * Cambridge, MA
 *
 * Algorithm available in XLS form from the Washing State Department of Ecology
 * SolRad: http://www.ecy.wa.gov/programs/eap/models.html
 *
 * @param solar_elevation Solar elevation in degrees from the horizon
 * @param sun_distance_au Distance from the sun to the earth in AU
 * @param transmission_factor Atmospheric transmission coefficient (0.7-0.91, default 0.8)
 * @param altitude Elevation in meters
 */
function ryan_stolzenbach_solar(solar_elevation, sun_distance_au, transmission_factor, altitude) {
    // Solar constant in W/m^2
    var solar_constant = 1367.0;
    
    var sin_solar_elevation_rad = sin_d(solar_elevation);

    if (sin_solar_elevation_rad < 0) {
        return 0.0; // Sun below horizon. No solar radiation.
    } else {
        var rm = Math.pow(((288.0 - 0.0065 * altitude) / 288.0), 5.256) 
            / (sin_solar_elevation_rad + 0.15 * Math.pow((degrees(asin(sin_solar_elevation_rad)) + 3.885), -1.253));

        // Solar Radiation on the top of the atmosphere
        var sun_rad_at_top_of_atmosphere = solar_constant * sin_solar_elevation_rad / Math.pow(sun_distance_au, 2);
        
        // Solar Radiation on the ground
        return sun_rad_at_top_of_atmosphere * Math.pow(transmission_factor, rm)
    }
}

var SOLAR_ALG_BRAS = 0;
var SOLAR_ALG_RYAN_STOLZENBACH = 1;

/** Calculate maximum solar radiation using one of a number of algorithms for a particular
 * time and location.
 * 
 * @param algorithm Algorithm to use
 * @param at_time Time to calculate for
 * @param latitude Latitude in degrees
 * @param longitude Longitude in degrees
 * @param altitude Altitude (required for Ryan-Stolzenbach algorithm only)
 * @param transmission_factor Atmospheric transmission coefficient (0.7-0.91, default 0.8). 
 *                            For Ryan-Stolzenbach algorithm only.
 * @param turbidity Atmospheric turbidity (2=clear, 4-5=smoggy). For Bras algorithm only.
 */
function solar_max(algorithm, at_time, latitude, longitude, altitude, transmission_factor, 
                   turbidity) {
    
    // All calculations are done at UTC.
    var at_utc = at_time.clone().tz('UTC');
    
    var sun_position = solar_position(at_utc, latitude, longitude);
    
    var solar_elevation = sun_position.corrected_altitude;
    var sun_distance_au = sun_position.distance;
    
    if (algorithm === SOLAR_ALG_BRAS) {
        return bras_solar(solar_elevation, sun_distance_au, turbidity);
    } else if (algorithm === SOLAR_ALG_RYAN_STOLZENBACH) {
        return ryan_stolzenbach_solar(solar_elevation, sun_distance_au, transmission_factor, altitude);
    }
}