/** JavaScript for live data refresh and records refresh.
 * User: David Goodwin
 * Date: 21/05/12
 * Time: 6:31 PM
 */

var previous_live = null;

var socket = null;
var ws_connected = false;
var ws_state = 'conn';

function live_data_arrived(data) {
    var parts = data.split(',');

    if (parts[0] != 'l') {
        // Not a live record
        return;
    }

    if (parts.length < 12) {
        // Invalid live record (needs at least 12 fields)
        return;
    }

    var now = new Date();
    var h = now.getHours();
    var m = now.getMinutes();
    var s = now.getSeconds();
    if (h < 10) h = '0' + h;
    if (m < 10) m = '0' + m;
    if (s < 10) s = '0' + s;
    var time = h + ':' + m + ':' + s;

    var result = {
        'temperature': parseFloat(parts[1]),
        'dew_point': parseFloat(parts[2]),
        'apparent_temperature': parseFloat(parts[3]),
        'wind_chill': parseFloat(parts[4]),
        'relative_humidity': parseInt(parts[5]),
        // Indoor temperature - 6
        // Indoor humidity - 7
        'absolute_pressure': parseFloat(parts[8]),
        'average_wind_speed': parseFloat(parts[9]),
        // gust wind speed - 10 (wh1080-specific)
        'wind_direction': parseInt(parts[11]),
        'hw_type': hw_type,
        'time_stamp': time,
        'age': 0,
        's': 'ok',
        'davis': null
    };

    if (hw_type == 'DAVIS') {

        if (parts.length != 20) {
            // Invalid davis record (needs 20 fields)
            return;
        }

        result['davis'] = {
            'bar_trend': parseInt(parts[12]),
            'rain_rate': parseFloat(parts[13]),
            'storm_rain': parseFloat(parts[14]),
            'current_storm_date': parts[15],
            'tx_batt': parseInt(parts[16]),
            'console_batt': parseFloat(parts[17]),
            'forecast_icon': parseInt(parts[18]),
            'forecast_rule': parseInt(parts[19])
        };
    }

    refresh_live_data(result);
}

function poll_live_data() {
    $.getJSON(live_url, function (data) {
        refresh_live_data(data);
    }).error(function() {
                 $("#cc_refresh_failed").show();
                 $("#current_conditions").hide();
                 $("#cc_stale").hide();
             });
}


function ws_data_arrived(evt) {
    if (evt.data == '_ok\r\n' && ws_state == 'conn') {
        socket.send('subscribe "' + station_code + '"/live\r\n');
        ws_state = 'sub';
    } else if (ws_state == 'sub') {
        if (evt.data[0] != '#') {
            live_data_arrived(evt.data);
        }
    }
}


function ws_connect(evt) {
    ws_connected = true;
    socket.send('set client "zxw_web"/version="1.0.0"\r\n');
}


function ws_error(evt) {

}


function start_polling() {
    // Refresh live data every 48 seconds.
    window.setInterval(function(){
        poll_live_data();
    }, 30000);
}


function attempt_ws_connect() {
    socket = new WebSocket(ws_uri);
    socket.onmessage = ws_data_arrived;
    socket.onopen = ws_connect;
    socket.onerror = ws_error;
    socket.onclose = function(evt) {
        if (!ws_connected) {
            attempt_wss_connect();
        }
    };
}


function attempt_wss_connect() {
    socket = new WebSocket(wss_uri);
    socket.onmessage = ws_data_arrived;
    socket.onopen = ws_connect;
    socket.onerror = ws_error;
    socket.onclose = function(evt) {
        if (!ws_connected) {
            start_polling();
        }
    };
}


if (live_auto_refresh) {
     poll_live_data();

    if (window.MozWebSocket) {
         window.WebSocket = window.MozWebSocket;
    }

    if(window.WebSocket) {
        // Browser seems to support websockets. Try that if we can.
        if (ws_uri != null)
            attempt_ws_connect();
        else if (wss_uri != null)
            attempt_wss_connect();
        else
            start_polling();
    } else {
         start_polling();
    }
}


var wind_directions = [
    "N", "NNE", "NE", "ENE", "E", "ESE", "SE", "SSE", "S", "SSW", "SW",
    "WSW", "W", "WNW", "NW", "NNW"
];

/** Refreshes the live data display from the supplied dict.
 * @param data Data to populate the display with.
 */
function refresh_live_data(data) {
    $("#cc_refresh_failed").hide();

    var cc_bad = $("#cc_bad");
    var cc_stale = $("#cc_stale");
    var current_conditions = $("#current_conditions");

    cc_bad.hide();
    if (data['s'] == "bad") {
        current_conditions.hide();
        cc_bad.show();
    }
    // If the live data is over 5 minutes old show a warning instead.
    else if (data['age'] > 300) {
        $("#cc_data_age").html(data['age']);
        current_conditions.hide();
        cc_stale.show();
    } else {
        current_conditions.show();
        cc_stale.hide();

        // Current conditions
        var relative_humidity = data['relative_humidity'];
        var temperature = data['temperature'].toFixed(1);
        var apparent_temp = data['apparent_temperature'].toFixed(1);
        var wind_chill = data['wind_chill'].toFixed(1);
        var dew_point = data['dew_point'].toFixed(1);
        var absolute_pressure = data['absolute_pressure'].toFixed(1);
        var average_wind_speed = data['average_wind_speed'].toFixed(1);
        var hw_type = data['hw_type'];
        var wind_direction = data['wind_direction'];

        var bar_trend = '';

        if (hw_type == 'DAVIS') {
            var bar_trend_val = data['davis']['bar_trend'];
            if (bar_trend_val == -60)
                bar_trend = ' (falling rapidly)';
            else if (bar_trend_val == -20)
                bar_trend = ' (falling slowly)';
            else if (bar_trend_val == 0)
                bar_trend = ' (steady)';
            else if (bar_trend_val == 20)
                bar_trend = ' (rising slowly)';
            else if (bar_trend_val == 60)
                bar_trend = ' (rising rapidly)';
        }

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

        if (average_wind_speed == 0.0)
            wind_direction = '--';
        else {
            var index = Math.floor(((wind_direction * 100 + 1125) % 36000) / 2250);
            wind_direction += '\u00B0 (' + wind_directions[index] + ')'
        }

        $("#live_relative_humidity").html(rh + relative_humidity + '%');
        $("#live_temperature").html(t + temperature + '°C');
        $("#live_apparent_temperature").html(at + apparent_temp + '°C');
        $("#live_wind_chill").html(wc + wind_chill + '°C');
        $("#live_dew_point").html(dp + dew_point + '°C');
        $("#live_absolute_pressure").html(ap + absolute_pressure + ' hPa' + bar_trend);
        $("#live_avg_wind_speed").html(average_wind_speed + ' m/s');
        $("#live_wind_direction").html(wind_direction);
        $("#current_time").html(data['time_stamp']);

    }
    previous_live = data
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

