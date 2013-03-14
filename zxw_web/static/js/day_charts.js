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
    $("#chart_temperature_tdp_div").empty();
    $("#chart_temperature_awc_div").empty();
    $("#chart_humidity_div").empty();
    $("#chart_pressure_div").empty();
    $("#chart_wind_speed_div").empty();
    $("#chart_hourly_rainfall_div").empty();

    $("#chart_temperature_tdp_div").html('<div class="bg_loading"></div>');
    $("#chart_temperature_awc_div").html('<div class="bg_loading"></div>');
    $("#chart_humidity_div").html('<div class="bg_loading"></div>');
    $("#chart_pressure_div").html('<div class="bg_loading"></div>');
    $("#chart_wind_speed_div").html('<div class="bg_loading"></div>');
    $("#chart_hourly_rainfall_div").html('<div class="bg_loading"></div>');

    load_day_charts();
    show_hide_rainfall_charts(1); // 1 = 1day chart only
}


function refresh_7day_charts() {
    $("#chart_7_temperature_tdp_div").empty();
    $("#chart_7_temperature_awc_div").empty();
    $("#chart_7_humidity_div").empty();
    $("#chart_7_pressure_div").empty();
    $("#chart_7_wind_speed_div").empty();
    $("#chart_7_hourly_rainfall_div").empty();

    $("#chart_7_temperature_tdp_div").html('<div class="bg_loading"></div>');
    $("#chart_7_temperature_awc_div").html('<div class="bg_loading"></div>');
    $("#chart_7_humidity_div").html('<div class="bg_loading"></div>');
    $("#chart_7_pressure_div").html('<div class="bg_loading"></div>');
    $("#chart_7_wind_speed_div").html('<div class="bg_loading"></div>');
    $("#chart_7_hourly_rainfall_div").html('<div class="bg_loading"></div>');

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

