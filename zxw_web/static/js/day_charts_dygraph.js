/** Day page line charts implemented using the digraph library for line charts.
 *
 * User: David Goodwin
 * Date: 21/06/12
 * Time: 9:02 PM
 */

/* This function is stolen from the custom plotters exmaple:
 * http://dygraphs.com/tests/plotters.html
 */
function barChartPlotter(e) {
    var ctx = e.drawingContext;
    var points = e.points;
    var y_bottom = e.dygraph.toDomYCoord(0);

    // The RGBColorParser class is provided by rgbcolor.js, which is
    // packed in with dygraphs.
    var color = new RGBColorParser(e.color);
    color.r = Math.floor((255 + color.r) / 2);
    color.g = Math.floor((255 + color.g) / 2);
    color.b = Math.floor((255 + color.b) / 2);
    ctx.fillStyle = color.toRGB();

    // Find the minimum separation between x-values.
    // This determines the bar width.
    var min_sep = Infinity;
    for (var i = 1; i < points.length; i++) {
        var sep = points[i].canvasx - points[i - 1].canvasx;
        if (sep < min_sep) min_sep = sep;
    }
    var bar_width = Math.floor(2.0 / 3 * min_sep);

    // Do the actual plotting.
    for (var i = 0; i < points.length; i++) {
        var p = points[i];
        var center_x = p.canvasx;

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
                           element,
                           labels_element) {

    var labels = jsondata['labels'];
    var data = jsondata['data'];

    /* Make sure the first column is Date objects, not strings */
    data = convertToDates(data);

    var chart = new Dygraph(element,
                            data,
                            {
                                plotter: barChartPlotter,
                                labels: labels,
                                labelsDiv: labels_element,
                                title: 'Rainfall (mm)',
                                legend: 'always',
                                axes: {
                                    y: {
                                        valueFormatter: rainfallFormatter
                                    }
                                },
                                xRangePad: 15
                                // Animated zooming must be off for xRangePad
                                // to have any effect.
                                //,animatedZooms: true
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

        drawSampleLineCharts(data,
            document.getElementById('chart_temperature_tdp_div'),
            document.getElementById('chart_temperature_tdp_key'),
            document.getElementById('chart_temperature_awc_div'),
            document.getElementById('chart_temperature_awc_key'),
            document.getElementById('chart_humidity_div'),
            document.getElementById('chart_humidity_key'),
            document.getElementById('chart_pressure_div'),
            document.getElementById('chart_pressure_key'),
            document.getElementById('chart_wind_speed_div'),
            document.getElementById('chart_wind_speed_key'),
            document.getElementById('chart_solar_radiation_div'),
            document.getElementById('chart_solar_radiation_key'),
            document.getElementById('chart_uv_index_div'),
            document.getElementById('chart_uv_index_key')
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

        drawRainfallChart(
            data,
            document.getElementById('chart_hourly_rainfall_div'),
            document.getElementById('chart_hourly_rainfall_key'));

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
            document.getElementById('chart_7_temperature_tdp_key'),
            document.getElementById('chart_7_temperature_awc_div'),
            document.getElementById('chart_7_temperature_awc_key'),
            document.getElementById('chart_7_humidity_div'),
            document.getElementById('chart_7_humidity_key'),
            document.getElementById('chart_7_pressure_div'),
            document.getElementById('chart_7_pressure_key'),
            document.getElementById('chart_7_wind_speed_div'),
            document.getElementById('chart_7_wind_speed_key'),
            document.getElementById('chart_7_solar_radiation_div'),
            document.getElementById('chart_7_solar_radiation_key'),
            document.getElementById('chart_7_uv_index_div'),
            document.getElementById('chart_7_uv_index_key')
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

        drawRainfallChart(
            data,
            document.getElementById('chart_7_hourly_rainfall_div'),
            document.getElementById('chart_7_hourly_rainfall_key'));

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


if (Modernizr.canvas)
    drawCharts();
else {
    var msg = '<strong>Unsupported browser!</strong> These charts can not be drawn because your browser does not support the required features.';
    $('#lc7_msg').html(msg);
    $('#lc_msg').html(msg);

    if (typeof ie8 === 'undefined')
        var ie8 = false;

    // Only show the Canvas warning if the user isn't using IE8. For IE8 we have
    // a different warning message as the Alternate interface has some performance
    // issues there too.
    if (!ie8)
        $('#canvas_missing').show();

    $('#lc7_obsolete_browser').show();
    $('#lc_obsolete_browser').show();
    $('#7day-charts').hide();
    $('#day_charts_cont').hide();
}