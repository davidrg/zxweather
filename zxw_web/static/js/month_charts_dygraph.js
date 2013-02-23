/** Month charts implemented with Dygraphs
 *
 * User: David Goodwin
 * Date: 21/06/12
 * Time: 10:06 PM
 */

function drawCharts() {
    /***************************************************************
     * Fetch the days samples and draw the 1-day charts
     */
    $.getJSON(samples_url, function(data) {

        drawSampleLineCharts(
            data,
            document.getElementById('chart_temperature_tdp_div'),
            document.getElementById('chart_temperature_awc_div'),
            document.getElementById('chart_humidity_div'),
            document.getElementById('chart_pressure_div'),
            document.getElementById('chart_wind_speed_div')
        );
    });

    $.getJSON(daily_records_url, function(data) {

        drawRecordsLineCharts(
            data,
            document.getElementById('chart_rec_temperature'),
            document.getElementById('chart_rec_pressure'),
            document.getElementById('chart_rec_humidity'),
            document.getElementById('chart_rainfall'),
            document.getElementById('chart_rec_wind_speed')
        );
    });
}

if (Modernizr.canvas)
    drawCharts();
else {
    var msg = '<strong>Unsupported browser!</strong> These charts can not be drawn because your browser does not support the required features.';
    $('#lcr_msg').html(msg);
    $('#lc_msg').html(msg);

    // Only show the Canvas warning if the user isn't using IE8. For IE8 we have
    // a different warning message as the Alternate interface has some performance
    // issues there too.
    if (!($.browser.msie && ($.browser.version == '8.0' || $.browser.version == '7.0')))
        $('#canvas_missing').show();

    $('#lcr_obsolete_browser').show();
    $('#lc_obsolete_browser').show();
    $('#month_charts').hide();
    $('#records_charts').hide();
}