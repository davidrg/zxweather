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

drawCharts();