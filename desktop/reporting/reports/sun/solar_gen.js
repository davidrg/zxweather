function log(text) {
    console.log(text);
}

function degrees(angle) {
    return angle * (180 / Math.PI);
}

function radians(angle) {
    return angle * (Math.PI / 180);
}

function cos(angle) {
    return Math.cos(angle);
}

function acos(angle) {
    return Math.acos(angle);
}

function sin(angle) {
    return Math.sin(angle);
}

function asin(angle) {
    return Math.asin(angle);
}

function tan(angle) {
    return Math.tan(angle);
}

function mod(a, b) {
    return a % b;
}

/** Geneates a list of dates starting on start_date and ending on end_date.
 */
function generate_date_series(start_date, end_date) {
    log("====Generate Date Series====");
    log("Start date: " + start_date.format());
    log("End date: " + end_date.format());

    var dates = [];
    var last_date = start_date.clone();

    while (last_date < end_date) {
        dates.push(last_date.clone());
        last_date = last_date.clone().add(1, 'days');
    }
    
    log("============================\n");
    return dates;
}


function julian_date(date) {
    return (date.unix() / 86400) + 2440587.5; 
}


/**
 * @param jdate Julian date
 */
function julian_century(jdate) {
    return (jdate - 2451545) / 36525;
}


function calculate_for_date(date, latitude, longitude, time_offset_minutes) {
    var jdate = julian_date(date);
    var jcentury = julian_century(jdate);

    var mean_obliq_eliptic_deg = 23 + (26 + ((21.448 - jcentury * (46.815 + jcentury * (0.00059 - jcentury * 0.001813)))) / 60) / 60;
    var obliq_corr_deg = mean_obliq_eliptic_deg + 0.00256 * cos(radians(125.04 - 1934.136 * jcentury));
    var geom_mean_long_sun_deg = mod((280.46646 + jcentury * (36000.76983 + jcentury * 0.0003032)), 360.0);
    var geom_mean_anom_sun_deg = 357.52911 + jcentury * (35999.05029 - 0.0001537 * jcentury);
    var eccent_earth_orbit = 0.016708634 - jcentury * (0.000042037 + 0.0000001267 * jcentury);
    var sun_eq_of_ctr = sin(radians(geom_mean_anom_sun_deg))
        * (1.914602 - jcentury * (0.004817 + 0.000014 * jcentury))
        + sin(radians(2 * geom_mean_anom_sun_deg)) * (0.019993 - 0.000101 * jcentury)
        + sin(radians(3 * geom_mean_anom_sun_deg)) * 0.000289;
    var sun_true_long_deg = geom_mean_long_sun_deg + sun_eq_of_ctr;
    var sun_app_long_deg = sun_true_long_deg - 0.00569 - 0.00478 * sin(radians(125.04 - 1934.136 * jcentury));
    var sun_decln_deg = degrees(asin(sin(radians(obliq_corr_deg)) * sin(radians(sun_app_long_deg))));
    var var_y = tan(radians(obliq_corr_deg / 2)) * tan(radians(obliq_corr_deg / 2));

    var eq_of_time_minutes = 4 * degrees(var_y * sin(2 * radians(geom_mean_long_sun_deg))
        - 2 * eccent_earth_orbit * sin(radians(geom_mean_anom_sun_deg))
        + 4 * eccent_earth_orbit * var_y * sin(radians(geom_mean_anom_sun_deg))
        * cos(2 * radians(geom_mean_long_sun_deg)) - 0.5 * var_y * var_y
        * sin(4 * radians(geom_mean_long_sun_deg)) - 1.25 * eccent_earth_orbit * eccent_earth_orbit
        * sin(2 * radians(geom_mean_anom_sun_deg)));

    var solar_noon_lst = (720 - 4 * longitude - eq_of_time_minutes + time_offset_minutes) / 1440;

    // Hour angles in degrees
    var ha_sunrise_deg = degrees(acos(cos(radians(90.833)) / (cos(radians(latitude)) * cos(radians(sun_decln_deg))) - tan(radians(latitude)) * tan(radians(sun_decln_deg))));
    var ha_civil_dawn = degrees(acos(cos(radians(96.0)) / (cos(radians(latitude)) * cos(radians(sun_decln_deg))) - tan(radians(latitude)) * tan(radians(sun_decln_deg))));
    var ha_nautical_dawn = degrees(acos(cos(radians(102.0)) / (cos(radians(latitude)) * cos(radians(sun_decln_deg))) - tan(radians(latitude)) * tan(radians(sun_decln_deg))));
    var ha_astronomical_dawn = degrees(acos(cos(radians(108.0)) / (cos(radians(latitude)) * cos(radians(sun_decln_deg))) - tan(radians(latitude)) * tan(radians(sun_decln_deg))));

    // Times - these are all the number of seconds since midnight on the date.
    var solar_noon = solar_noon_lst * 86400;
    var sunrise_time = (((solar_noon_lst * 1440 - ha_sunrise_deg * 4) / 1440) * 86400);
    var sunset_time = (((solar_noon_lst * 1440 + ha_sunrise_deg * 4) / 1440) * 86400);
    var civil_dawn = (((solar_noon_lst * 1440 - ha_civil_dawn * 4) / 1440) * 86400);
    var civil_dusk = (((solar_noon_lst * 1440 + ha_civil_dawn * 4) / 1440) * 86400);
    var nautical_dawn = (((solar_noon_lst * 1440 - ha_nautical_dawn * 4) / 1440) * 86400);
    var nautical_dusk = (((solar_noon_lst * 1440 + ha_nautical_dawn * 4) / 1440) * 86400);
    var astronomical_dawn = (((solar_noon_lst * 1440 - ha_astronomical_dawn * 4) / 1440) * 86400);
    var astronomical_dusk = (((solar_noon_lst * 1440 + ha_astronomical_dawn * 4) / 1440) * 86400);


    return {
        date: date,
        solar_noon: date.clone().add(solar_noon, 'seconds'),
        sunrise: date.clone().add(sunrise_time, 'seconds'),
        sunset: date.clone().add(sunset_time, 'seconds'),
        civil_dawn: date.clone().add(civil_dawn, 'seconds'),
        nautical_dawn: date.clone().add(nautical_dawn, 'seconds'),
        nautical_dusk: date.clone().add(nautical_dusk, 'seconds'),
        astronomical_dawn: date.clone().add(astronomical_dawn, 'seconds'),
        astronomical_dusk: date.clone().add(astronomical_dusk, 'seconds'),
        civil_dusk: date.clone().add(civil_dusk, 'seconds'),
        day_length: sunset_time - sunrise_time,
        utc_offset: time_offset_minutes
    };
}

function short_time(time) {
    // Format is " 5:23a" or "10:14p", etc.

    var t = time.format("h:mm");
    if (time.format("a") === "am") {
        t += "a";
    } else {
        t += "p";
    }

    if (t.length < 6) {
        t = " " + t;
    }

    return t;
}

function generate_solar_dataset(criteria) {
	log("generating");
    log("Report Criteria:");
    for (var k in criteria){
        if (criteria.hasOwnProperty(k)) {
             log(k + " = " + criteria[k]);
        }
    }
    log("\n");

    /* Params */
    var timeZone = criteria["timeZone"];
    var start = moment.unix(criteria["start_t"]);
    var end = moment.unix(criteria["end_t"]).hour(23);

     var latitude = criteria["latitude"];
    var longitude = criteria["longitude"];

    log("Running report with following settings:");
    log("Latitude: " + latitude);
    log("Longitude: " + longitude);
    log("Start: " + start.format() + "\nEnd: " + end.format() + "\nTime Zone: " + timeZone);
    log("\n");

    var series = generate_date_series(start.clone(), end.clone());

    log("Date Series: ");
    for(var x = 0; x < series.length; x++) {
        log(series[x].format() + "\t" + series[x].clone().tz(timeZone).format() + "\t" + series[x].clone().tz(timeZone).utcOffset())
    }



    var results = [];
    var prev_day_length = null;
    var prev_offset = null;
    for (var i = 0; i < series.length; i++) {
        var result = calculate_for_date(series[i], latitude, longitude, series[i].clone().tz(timeZone).utcOffset());

        var day_length_change = "";
        var this_day_length = result.day_length;

        // For the first row in the output we need to calculate data
        if (prev_day_length == null) {
            var prev_date = result.date.clone().subtract(1, 'days');
            prev_day_length = calculate_for_date(prev_date, latitude, longitude, prev_date.clone().tz(timeZone).utcOffset()).day_length;

            prev_offset = prev_date.clone().tz(timeZone).hours(12).utcOffset();
        }

        // Calculate change in day length from previous day
        var delta = 0;
        if (this_day_length > prev_day_length) {
            day_length_change = "+";
            delta = this_day_length - prev_day_length;
            day_length_change += moment.unix(0).add(delta, 'seconds').format('mm:ss');
        } else if (this_day_length < prev_day_length ) {
            day_length_change = "-";
             delta = prev_day_length - this_day_length;
            day_length_change += moment.unix(0).add(delta, 'seconds').format('mm:ss');
        } else {
            day_length_change = "0:0";
        }

        var this_date = result.date.clone().tz(timeZone);
        var this_date_str = this_date.format("YYYY-MM-DD");

        var isToday = moment().format("YYYY-MM-DD") === this_date_str;

        var this_offset = this_date.clone().tz(timeZone).hours(12).utcOffset();
        var offset_change = this_offset - prev_offset;
        prev_offset = this_offset;

        var offset_change_message = "";
        if (offset_change !== 0) {
            var change = moment.unix(0).utc().add(Math.abs(offset_change), 'minutes');
            var hours = change.hours();
            var minutes = change.minutes();

            var hours_str = "";
            if (hours === 0) {
                hours_str = "";
            } else if (hours === 1) {
                hours_str = "1 hour";
            } else {
                hours_str = hours + " hours";
            }

            var minutes_str = "";
            if (minutes === 0) {
                minutes_str = "";
            } else {
                minutes_str = minutes + " minutes";
            }

            var direction = "foreward";
            if (offset_change < 0) {
                direction = "backward";
            }

            offset_change_message = "Note: hours shift because clocks change " + direction + " ";
            offset_change_message += hours_str;

            if (hours_str !== "" && minutes_str !== "") {
                offset_change_message += " and ";
            }

            offset_change_message += minutes_str;
            offset_change_message += ".";

        }

        var sunrise = result.sunrise.clone().tz(timeZone);
        var sunset = result.sunset.clone().tz(timeZone);
        var daylength = moment.unix(0).utc().add(result.day_length, 'seconds').format("HH:mm");
        var solarnoon = result.solar_noon.clone().tz(timeZone);
        var civil_dawn = result.civil_dawn.clone().tz(timeZone);
        var civil_dusk = result.civil_dusk.clone().tz(timeZone);
        var nautical_dawn = result.nautical_dawn.clone().tz(timeZone);
        var nautical_dusk = result.nautical_dusk.clone().tz(timeZone);
        var astro_dawn = result.astronomical_dawn.clone().tz(timeZone);
        var astro_dusk = result.astronomical_dusk.clone().tz(timeZone);

        var row = [
            this_date_str,
            sunrise.format("h:mm:ss a"),
            sunset.format("h:mm:ss a"),
            daylength,
            day_length_change,
            solarnoon.format("h:mm:ss a"),
            civil_dawn.format("h:mm:ss a"),
            civil_dusk.format("h:mm:ss a"),
            nautical_dawn.format("h:mm:ss a"),
            nautical_dusk.format("h:mm:ss a"),
            astro_dawn.format("h:mm:ss a"),
            astro_dusk.format("h:mm:ss a"),
            isToday,
            result.utc_offset,
            offset_change,
            offset_change_message,

            // Extra columns for plain-text output
            this_date.format("DD/MM/YY"),
            short_time(astro_dawn),
            short_time(nautical_dawn),
            short_time(civil_dawn),
            short_time(sunrise),
            short_time(sunset),
            short_time(civil_dusk),
            short_time(nautical_dusk),
            short_time(astro_dusk)
        ];
        results.push(row);
        prev_day_length = this_day_length;
    }

    // Dumps a list of all supported timezones to the console
    // var zones = moment.tz.names();
    // for (var zid = 0; zid < zones.length; zid++) {
    //     log(zones[zid]);
    // }

	return {
		"name": "solar",
		"column_names": [
			"date",
			"rise_time",
			"set_time",
			"daylength",
			"daylength_difference",
			"solar_noon",
			"civil_dawn",
			"civil_dusk",
			"nautical_dawn",
			"nautical_dusk",
			"astronomical_dawn",
			"astronomical_dusk",
			"today",
			"Offset",
			"Offset Change",
			"offset_change_message",

			"date_ddmmyy",
			"astro_dawn_short",
			"naut_dawn_short",
			"civil_dawn_short",
			"sunrise_short",
			"sunset_short",
			"civil_dusk_short",
			"naut_dusk_short",
			"astro_dusk_short"
		],
		"row_data": results
	};
}

