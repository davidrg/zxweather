/** Day page line charts implemented using the digraph library for line charts.
 *
 * User: David Goodwin
 * Date: 21/06/12
 * Time: 9:02 PM
 */

function drawAllLineCharts(jsondata,
                           tdp_element,
                           awc_element,
                           humidity_element,
                           pressure_element,
                           wind_speed_element) {


    var labels = jsondata['labels'];
    var data = jsondata['data'];

    /* Make sure the first column is Date objects, not strings */
    data = convertToDates(data);

    /* Split out the data:
     *  0 - Timestamp
     *  1 - Temperature
     *  2 - Dew Point
     *  3 - Apparent Temperature
     *  4 - Wind Chill
     *  5 - Humidity
     *  6 - Absolute Pressure
     *  7 - Average Wind Speed
     *  8 - Gust Wind Speed
     */
    var tdp_data = selectColumns(data, [0,1,2]);
    var awc_data = selectColumns(data, [0,3,4]);
    var humidity_data = selectColumns(data, [0,5]);
    var pressure_data = selectColumns(data, [0,6]);
    var wind_speed_data = selectColumns(data, [0,7,8]);

    /* And the labels */
    var tdp_labels = [labels[0],labels[1],labels[2]];
    var awc_labels = [labels[0],labels[3],labels[4]];
    var humidity_labels = [labels[0],labels[5]];
    var pressure_labels = [labels[0],labels[6]];
    var wind_speed_labels = [labels[0],labels[7],labels[8]];

    /* Settings */
    var enable_animated_zooms = true;
    var strokeWidth = 1.25;

    /* Formatting functions */
    var temperatureFormatter = function(y) {
        return y.toFixed(1) + '°C';
    };
    var humidityFormatter = function(y) {
        return y + '%';
    };
    var pressureFormatter = function(y) {
        return y.toFixed(1) + ' hPa';
    };
    var windSpeedFormatter = function(y) {
        return y.toFixed(1) + ' m/s';
    };

    /* Now chart it all */
    var tdp_chart = new Dygraph(tdp_element,
                                tdp_data,
                                {
                                    labels: tdp_labels,
                                    animatedZooms: enable_animated_zooms,
                                    strokeWidth: strokeWidth,
                                    title: 'Temperature and Dew Point (°C)',
                                    axes: {
                                        y: {
                                            valueFormatter: temperatureFormatter
                                        }
                                    }
                                });
    var awc_chart = new Dygraph(awc_element,
                                awc_data,
                                {
                                    labels: awc_labels,
                                    animatedZooms: enable_animated_zooms,
                                    strokeWidth: strokeWidth,
                                    title: 'Apparent Temperature and Wind Chill (°C)',
                                    axes: {
                                        y: {
                                            valueFormatter: temperatureFormatter
                                        }
                                    }
                                });
    var humidity_chart = new Dygraph(humidity_element,
                                     humidity_data,
                                     {
                                         labels: humidity_labels,
                                         animatedZooms: enable_animated_zooms,
                                         strokeWidth: strokeWidth,
                                         title: 'Humidity (%)',
                                         axes: {
                                             y: {
                                                 valueFormatter: humidityFormatter
                                             }
                                         }
                                     });
    var pressure_chart = new Dygraph(pressure_element,
                                     pressure_data,
                                     {
                                         labels: pressure_labels,
                                         animatedZooms: enable_animated_zooms,
                                         strokeWidth: strokeWidth,
                                         title: 'Absolute Pressure (hPa)',
                                         axes: {
                                             y: {
                                                 valueFormatter: pressureFormatter
                                             }
                                         }
                                     });
    var wind_speed_chart = new Dygraph(wind_speed_element,
                                       wind_speed_data,
                                       {
                                           labels: wind_speed_labels,
                                           animatedZooms: enable_animated_zooms,
                                           strokeWidth: strokeWidth,
                                           title: 'Wind Speed (m/s)',
                                           axes: {
                                               y: {
                                                   valueFormatter: windSpeedFormatter
                                               }
                                           }
                                       });
}

function load_day_charts() {
    $("#day_charts_cont").show();
    $("#lc_refresh_failed").hide();
    $("#btn_today_refresh").button('loading');
    samples_loading = true;
    rainfall_loading = true;

    /***************************************************************
     * Fetch the days samples and draw the 1-day charts
     */
    $.getJSON(samples_url, function(data) {


        drawAllLineCharts(data,
                          document.getElementById('chart_temperature_tdp_div'),
                          document.getElementById('chart_temperature_awc_div'),
                          document.getElementById('chart_humidity_div'),
                          document.getElementById('chart_pressure_div'),
                          document.getElementById('chart_wind_speed_div')
        );

        samples_loading = false;

        // Turn the refresh button back on if we're finished loading
        if (!samples_loading && !rainfall_loading)
            $("#btn_today_refresh").button('reset');
    }).error(function() {
                 $("#day_charts_cont").hide();
                 $("#lc_refresh_failed").show();
                 $("#btn_today_refresh").button('reset');
             });

}

function load_7day_charts() {
    $("#7day-charts").show();
    $("#lc7_refresh_failed").hide();
    samples_7_loading = true;
    rainfall_7_loading = true;
    $("#btn_7day_refresh").button('loading');

    /***************************************************************
     * Fetch samples for the past 7 days and draw the 7-day charts
     */
    $.getJSON(samples_7day_url, function(data) {

        drawAllLineCharts(data,
                          document.getElementById('chart_7_temperature_tdp_div'),
                          document.getElementById('chart_7_temperature_awc_div'),
                          document.getElementById('chart_7_humidity_div'),
                          document.getElementById('chart_7_pressure_div'),
                          document.getElementById('chart_7_wind_speed_div')
        );

        samples_7_loading = false;

        // Turn the refresh button back on if we're finished loading
        if (!samples_7_loading && !rainfall_7_loading)
            $("#btn_7day_refresh").button('reset');
    }).error(function() {
                 $("#7day-charts").hide();
                 $("#lc7_refresh_failed").show();
             });
}

drawCharts();