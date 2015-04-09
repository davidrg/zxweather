/** JavaScript for day charts
 * User: David Goodwin
 * Date: 19/05/12
 * Time: 11:34 PM
 */

var samples_loading = true;
var rainfall_loading = true;
var samples_7_loading = true;
var rainfall_7_loading = true;

function refresh_day_charts() {
    var chart_temperature_tdp_div = $("#chart_temperature_tdp_div");
    var chart_temperature_awc_div = $("#chart_temperature_awc_div");
    var chart_humidity_div = $("#chart_humidity_div");
    var chart_pressure_div = $("#chart_pressure_div");
    var chart_wind_speed_div = $("#chart_wind_speed_div");
    var chart_hourly_rainfall_div = $("#chart_hourly_rainfall_div");
    var chart_solar_radiation_div = $("#chart_solar_radiation_div");
    var chart_uv_index_div = $("#chart_uv_index_div");

    chart_temperature_tdp_div.empty();
    chart_temperature_awc_div.empty();
    chart_humidity_div.empty();
    chart_pressure_div.empty();
    chart_wind_speed_div.empty();
    chart_hourly_rainfall_div.empty();
    chart_solar_radiation_div.empty();
    chart_uv_index_div.empty();

    var loading_div = '<div class="bg_loading"></div>';
    chart_temperature_tdp_div.html(loading_div);
    chart_temperature_awc_div.html(loading_div);
    chart_humidity_div.html(loading_div);
    chart_pressure_div.html(loading_div);
    chart_wind_speed_div.html(loading_div);
    chart_hourly_rainfall_div.html(loading_div);
    chart_solar_radiation_div.html(loading_div);
    chart_uv_index_div.html(loading_div);

    load_day_charts();
    show_hide_rainfall_charts(1); // 1 = 1day chart only
}


function refresh_7day_charts() {
    var chart_7_temperature_tdp_div = $("#chart_7_temperature_tdp_div");
    var chart_7_awc_div = $("#chart_7_temperature_awc_div");
    var chart_7_humidity_div = $("#chart_7_humidity_div");
    var chart_7_pressure_div = $("#chart_7_pressure_div");
    var chart_7_wind_speed_div = $("#chart_7_wind_speed_div");
    var chart_7_hourly_rainfall_div = $("#chart_7_hourly_rainfall_div");
    var chart_7_solar_radiation_div = $("#chart_7_solar_radiation_div");
    var chart_7_uv_index_div = $("#chart_7_uv_index_div");

    chart_7_temperature_tdp_div.empty();
    chart_7_awc_div.empty();
    chart_7_humidity_div.empty();
    chart_7_pressure_div.empty();
    chart_7_wind_speed_div.empty();
    chart_7_hourly_rainfall_div.empty();
    chart_7_solar_radiation_div.empty();
    chart_7_uv_index_div.empty();

    var loading_div = '<div class="bg_loading"></div>';
    chart_7_temperature_tdp_div.html(loading_div);
    chart_7_awc_div.html(loading_div);
    chart_7_humidity_div.html(loading_div);
    chart_7_pressure_div.html(loading_div);
    chart_7_wind_speed_div.html(loading_div);
    chart_7_hourly_rainfall_div.html(loading_div);
    chart_7_solar_radiation_div.html(loading_div);
    chart_7_uv_index_div.html(loading_div);

    load_7day_charts();
    show_hide_rainfall_charts(2); // 2 = 7day chart only
}

// Checks the server to see if there has been any rainfall for either of the
// charts. If there hasn't they are hidden.
// chart values:  0 - both charts
//                1 - day chart only
//                2 - 7 day chart only
function show_hide_rainfall_charts(chart) {
    $.getJSON(rainfall_totals_url, function(data) {
        if (chart == 0 || chart == 1) {
            if (data['rainfall'] > 0) {
                $("#chart_hourly_rainfall_div").show();
                $("#chart_hourly_rainfall_key").show();
            } else {
                $("#chart_hourly_rainfall_div").hide();
                $("#chart_hourly_rainfall_key").hide();
            }
            $("#tot_rainfall").html(data['rainfall'].toFixed(1));
        }

        if (chart == 0 || chart == 2) {
            if (data['7day_rainfall'] > 0) {
                $("#chart_7_hourly_rainfall_div").show();
                $("#chart_7_hourly_rainfall_key").show();
            } else {
                $("#chart_7_hourly_rainfall_div").hide();
                $("#chart_7_hourly_rainfall_key").hide();
            }
        }
    }).error(function() {
        $("#chart_hourly_rainfall_div").hide();
        $("#chart_7_hourly_rainfall_div").hide();
    });
}

/** Draws all charts on the page.
 *
 * This is called by either:
 *   The Google Visualisation API when it has finished loading all its stuff
 *   at the end of day_charts_gviz.js
 * OR
 *   day_charts_dygraph.js at the end.
 *
 */
function drawCharts() {
    load_day_charts();
    load_7day_charts();
    show_hide_rainfall_charts(0); // 0 = both charts
}

