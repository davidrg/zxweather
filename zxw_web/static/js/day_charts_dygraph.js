/** Day page line charts implemented using the digraph library for line charts.
 *
 * User: David Goodwin
 * Date: 21/06/12
 * Time: 9:02 PM
 */

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


        drawSampleLineCharts(data,
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

        drawSampleLineCharts(data,
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