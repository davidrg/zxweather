/** Year charts implemented with Dygraphs.
 *
 * User: David Goodwin
 * Date: 2/8/2012
 * Time: 6:53 PM
 */

function drawCharts() {
    $.getJSON(daily_records_url, function(data) {

        drawRecordsLineCharts(
            data,
            document.getElementById('chart_rec_temperature'),
            document.getElementById('key_rec_temperature'),
            document.getElementById('chart_rec_pressure'),
            document.getElementById('key_rec_pressure'),
            document.getElementById('chart_rec_humidity'),
            document.getElementById('key_rec_humidity'),
            document.getElementById('chart_rainfall'),
            document.getElementById('key_rainfall'),
            document.getElementById('chart_rec_wind_speed'),
            document.getElementById('key_rec_wind_speed')
        );
    }).error(function() {
            $("#records_charts").hide();
            $("#lcr_refresh_failed").show();
        });
}

if (Modernizr.canvas)
    drawCharts();
else {
    $('#lcr_msg').html('<strong>Unsupported browser!</strong> These charts can not be drawn because your browser does not support the required features.');

    // Only show the Canvas warning if the user isn't using IE8. For IE8 we have
    // a different warning message as the Alternate interface has some performance
    // issues there too.
    if (!($.browser.msie && ($.browser.version == '8.0' || $.browser.version == '7.0')))
        $('#canvas_missing').show();

    $('#lcr_obsolete_browser').show();
    $('#records_charts').hide();
}
