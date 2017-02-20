/** Day page line charts implemented using the digraph library for line charts.
 *
 * User: David Goodwin
 * Date: 21/06/12
 * Time: 9:02 PM
 */

if (!window.console) console = {log: function() {}};

function maybe_start_live_data() {
    if (!samples_loading && !samples_7_loading
        && !rainfall_loading && !rainfall_7_loading && live_auto_refresh) {
        console.log('Graph loading complete.');

        // On the standard UI the live data subscription waits for graphs to
        // finish loading so its able to auto-update them. Now that we've
        // finished loading let it know it can get started.
        start_day_live();
    }
}

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
 * @param jsondata *ordered* data to plot.
 * @param element Element to plot in.
 * @param labels_element the element to hold chart labels
 * @param is_hourly if the supplied data is hourly or 5-minutely
 */
function drawRainfallChart(jsondata,
                           element,
                           labels_element,
                           is_hourly) {

    var labels = jsondata['labels'];
    var raw_data = jsondata['data'];

    /* Make sure the first column is Date objects, not strings */
    raw_data = convertToDates(raw_data);

    var hourly_totals = [];

    var current_ts = null;
    var current_total = 0;

    if (is_hourly) {
        // The data is an hourly total already.
        hourly_totals = raw_data;
    } else {
        // We need to compute an hourly total. We also need to retain the data
        // used to do this so we can update the 24h rainfall total client-side.
        for(var i = 0; i < raw_data.length; i++) {
            var record = raw_data[i];
            var ts = new Date(record[0].getTime());
            var rainfall = record[1];

            ts.setMinutes(0);
            ts.setSeconds(0);
            ts.setMilliseconds(0);

            if (current_ts == null) {
                current_ts = ts;
            }

            if (current_ts.getTime() != ts.getTime()) {
                hourly_totals.push([current_ts, current_total]);
                current_ts = ts;
                current_total = 0;
            }

            current_total += rainfall;
        }
    }

    // Store the final hourly total
    if (current_ts != null) {
        hourly_totals.push([current_ts, current_total]);
    }

    var chart = new Dygraph(element,
                            hourly_totals,
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

    return {
        raw_data: raw_data,
        data: hourly_totals,
        _graph: chart,
        _element: $(element),
        _labels_element: $(labels_element),
        _hide: function() {
            this._element.hide();
            this._labels_element.hide();
        },
        _show: function() {
            this._element.show();
            this._labels_element.show();
        },
        _show_hide: function() {
                        // Automatically show or hide the graph depending on if it contains
            // any data.
            var total = 0;
            for (var i = 0; i < this.data.length; i++) {
                total += this.data[i][1]; // 0 is the timestamp, 1 is the data
            }

            console.log("Chart rain total " + total.toString());

            if (total > 0) {
                this._show();
            } else {
                console.log('Hiding rainfall chart (no data to show)');
                this._hide();
            }
        },
        update_graph: function() {
            this._graph.updateOptions({ 'file': this.data } );

            this._show_hide()
        }
    }
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
    $.getJSON(samples_url,  function(data) {

        data_sets.day = drawSampleLineCharts(data,
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
        maybe_start_live_data();

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

        data_sets.rainfall_day = drawRainfallChart(
            data,
            document.getElementById('chart_hourly_rainfall_div'),
            document.getElementById('chart_hourly_rainfall_key'),
            !live_auto_refresh); // 24h data set is every 5 minutes on live day pages

        data_sets.rainfall_day._show_hide();

        // Set 24h total rainfall thing:
        var total = 0;
        for (var i = 0; i < data_sets.rainfall_day.data.length; i++) {
            total += data_sets.rainfall_day.data[i][1];
        }
        $("#tot_rainfall").text(total.toFixed(1));

        rainfall_loading = false;
        maybe_start_live_data();

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

        data_sets.week = drawSampleLineCharts(data,
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
        maybe_start_live_data();

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

        data_sets.rainfall_week = drawRainfallChart(
            data,
            document.getElementById('chart_7_hourly_rainfall_div'),
            document.getElementById('chart_7_hourly_rainfall_key'),
            true); // 168h data set is hourly

        data_sets.rainfall_week._show_hide();

        rainfall_7_loading = false;
        maybe_start_live_data();

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