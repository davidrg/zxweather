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
                                    },
                                    legend: 'always',
                                    labelsDivStyles: {
                                        'text-align': 'right',
                                        'background': 'none'
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
                                    },
                                    legend: 'always',
                                    labelsDivStyles: {
                                        'text-align': 'right',
                                        'background': 'none'
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
                                         },
                                         legend: 'always',
                                         labelsDivStyles: {
                                             'text-align': 'right',
                                             'background': 'none'
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
                                         },
                                         legend: 'always',
                                         labelsDivStyles: {
                                             'text-align': 'right',
                                             'background': 'none'
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
                                           },
                                           legend: 'always',
                                           labelsDivStyles: {
                                               'text-align': 'right',
                                               'background': 'none'
                                           }
                                       });
}


/* This function is stolen from the dygraphs blog:
 * http://blog.dygraphs.com/2012/08/introducing-custom-plotters.html
 */
function barChartPlotter(e) {
    var ctx = e.drawingContext;
    var points = e.points;
    var y_bottom = e.dygraph.toDomYCoord(0);  // see http://dygraphs.com/jsdoc/symbols/Dygraph.html#toDomYCoord

    // This should really be based on the minimum gap
    var bar_width = 2/3 * (points[1].canvasx - points[0].canvasx);
    ctx.fillStyle = e.color;

    // Do the actual plotting.
    for (var i = 0; i < points.length; i++) {
        var p = points[i];
        var center_x = p.canvasx;  // center of the bar

        ctx.fillRect(center_x - bar_width / 2, p.canvasy,
                     bar_width, y_bottom - p.canvasy);
        ctx.strokeRect(center_x - bar_width / 2, p.canvasy,
                       bar_width, y_bottom - p.canvasy);
    }
}

/** Draws a rainfall chart.
 *
 * @param jsondata Data to plot.
 * @param element Element to plot in.
 */
function drawRainfallChart(jsondata,
                      element) {

    var labels = jsondata['labels'];
    var data = jsondata['data'];

    /* Make sure the first column is Date objects, not strings */
    data = convertToDates(data);

    /* Formatting functions */
    var rainfallFormatter = function(y) {
        return y.toFixed(1) + 'mm';
    };

    var chart = new Dygraph(element,
                            data,
                            {
                                plotter: barChartPlotter,
                                labels: labels,
                                title: 'Rainfall (mm)',
                                legend: 'always',
                                axes: {
                                    y: {
                                        valueFormatter: rainfallFormatter
                                    }
                                },
                                labelsDivStyles: {
                                    'text-align': 'right',
                                    'background': 'none'
                                },
                                animatedZooms: true
                            })
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


    /***************************************************************
     * Fetch the days hourly rainfall and chart it
     */
    $.getJSON(rainfall_url, function(data) {

        drawRainfallChart(data, document.getElementById('chart_hourly_rainfall_div'));

        rainfall_loading = false;

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
                 $("#btn_7day_refresh").button('reset');
             });

    /***************************************************************
     * Fetch the 7 day hourly rainfall and chart it
     */
    $.getJSON(rainfall_7day_url, function(data) {

        drawRainfallChart(data, document.getElementById('chart_7_hourly_rainfall_div'));

        rainfall_7_loading = false;

        // Turn the refresh button back on if we're finished loading
        if (!samples_7_loading && !rainfall_7_loading)
            $("#btn_7day_refresh").button('reset');
    }).error(function() {
        $("#7day-charts").hide();
        $("#lc7_refresh_failed").show();
        $("#btn_7day_refresh").button('reset');
    });
}

drawCharts();