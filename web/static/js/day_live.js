/** JavaScript for live data refresh and records refresh.
 * User: David Goodwin
 * Date: 21/05/12
 * Time: 6:31 PM
 */

var previous_live = null;

function refresh_live_data() {
    $.getJSON(live_url, function (data) {
        $("#cc_refresh_failed").hide();

        // If the live data is over 5 minutes old show a warning instead.
        if (data['age'] > 300) {
            $("#cc_data_age").html(data['age']);
            $("#current_conditions").hide();
            $("#cc_stale").show();
        } else {
            $("#current_conditions").show();
            $("#cc_stale").hide();

            // Current conditions
            var relative_humidity = data['relative_humidity'];
            var temperature = data['temperature'].toFixed(1);
            var apparent_temp = data['apparent_temperature'].toFixed(1);
            var wind_chill = data['wind_chill'].toFixed(1);
            var dew_point = data['dew_point'].toFixed(1);
            var absolute_pressure = data['absolute_pressure'].toFixed(1);
            var average_wind_speed = data['average_wind_speed'].toFixed(1);
            var gust_wind_speed = data['gust_wind_speed'].toFixed(1);

            // Icons
            var rh='', t='', at='', wc='', dp='', ap='';

            if (previous_live != null) {
                function get_ico(current, prev) {
                    if (is_day_page) {
                        if (current > prev)
                            return '<img src="../../../../../images/up.png" alt="increase" title="increase"/>&nbsp;';
                        else if (current == prev)
                            return '<img src="../../../../../images/no-change.png" alt="no change" title="no change"/>&nbsp;';
                        else
                            return '<img src="../../../../../images/down.png" alt="decrease" title="decrease"/>&nbsp;';
                    } else { // station overview
                        if (current > prev)
                            return '<img src="../../images/up.png" alt="increase" title="increase"/>&nbsp;';
                        else if (current == prev)
                            return '<img src="../../images/no-change.png" alt="no change" title="no change"/>&nbsp;';
                        else
                            return '<img src="../../images/down.png" alt="decrease" title="decrease"/>&nbsp;';
                    }
                }

                rh = get_ico(relative_humidity, previous_live['relative_humidity']);
                t = get_ico(temperature, previous_live['temperature'].toFixed(1));
                at = get_ico(apparent_temp, previous_live['apparent_temperature'].toFixed(1));
                wc = get_ico(wind_chill, previous_live['wind_chill'].toFixed(1));
                dp = get_ico(dew_point, previous_live['dew_point'].toFixed(1));
                ap = get_ico(absolute_pressure, previous_live['absolute_pressure'].toFixed(1));
            }

            $("#live_relative_humidity").html(rh + relative_humidity + '%');
            $("#live_temperature").html(t + temperature + '°C');
            $("#live_apparent_temperature").html(at + apparent_temp + '°C');
            $("#live_wind_chill").html(wc + wind_chill + '°C');
            $("#live_dew_point").html(dp + dew_point + '°C');
            $("#live_absolute_pressure").html(ap + absolute_pressure + ' hPa');
            $("#live_avg_wind_speed").html(average_wind_speed + ' m/s');
            $("#live_gust_wind_speed").html(gust_wind_speed + ' m/s');
            $("#live_wind_direction").html(data['wind_direction']);
            $("#current_time").html(data['time_stamp']);

        }
        previous_live = data
    }).error(function() {
                 $("#cc_refresh_failed").show();
                 $("#current_conditions").hide();
                 $("#cc_stale").hide();
             });
}
if (live_auto_refresh) {
    refresh_live_data();

    // Refresh live data every 48 seconds.
    window.setInterval(function(){
        refresh_live_data();
    }, 48000);
}

function refresh_records() {
    $("#btn_records_refresh").button('loading');
    $.getJSON(records_url, function(data) {

        $("#records_table").show();
        $("#rec_refresh_failed").hide();

        $("#temp_min").html(data['min_temperature'].toFixed(1) + '°C at ' + data['min_temperature_ts']);
        $("#temp_max").html(data['max_temperature'].toFixed(1) + '°C at ' + data['max_temperature_ts']);

        $("#wc_min").html(data['min_wind_chill'].toFixed(1) + '°C at ' + data['min_wind_chill_ts']);
        $("#wc_max").html(data['max_wind_chill'].toFixed(1) + '°C at ' + data['max_wind_chill_ts']);

        $("#at_min").html(data['min_apparent_temperature'].toFixed(1) + '°C at ' + data['min_apparent_temperature_ts']);
        $("#at_max").html(data['max_apparent_temperature'].toFixed(1) + '°C at ' + data['max_apparent_temperature_ts']);

        $("#dp_min").html(data['min_dew_point'].toFixed(1) + '°C at ' + data['min_dew_point_ts']);
        $("#dp_max").html(data['max_dew_point'].toFixed(1) + '°C at ' + data['max_dew_point_ts']);

        $("#ap_min").html(data['min_absolute_pressure'].toFixed(1) + ' hPa at ' + data['min_absolute_pressure_ts']);
        $("#ap_max").html(data['max_absolute_pressure'].toFixed(1) + ' hPa at ' + data['max_absolute_pressure_ts']);

        $("#rh_min").html(data['min_humidity'] + '% at ' + data['min_humidity_ts']);
        $("#rh_max").html(data['max_humidity'] + '% at ' + data['max_humidity_ts']);

        $("#gws").html(data['max_gust_wind_speed'].toFixed(1) + ' m/s at ' + data['max_gust_wind_speed_ts']);
        $("#aws").html(data['max_average_wind_speed'].toFixed(1) + ' m/s at ' + data['max_average_wind_speed_ts']);

        $("#tot_rainfall").html(data['total_rainfall'].toFixed(1));
        $("#btn_records_refresh").button('reset');
    }).error(function() {
                 $("#records_table").hide();
                 $("#rec_refresh_failed").show();
                 $("#btn_records_refresh").button('reset');
             });
}

