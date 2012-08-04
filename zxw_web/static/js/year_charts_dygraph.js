/** Year charts implemented with Dygraphs.
 *
 * User: David Goodwin
 * Date: 2/8/2012
 * Time: 6:53 PM
 */

function drawCharts() {
    $.getJSON(daily_records_url, function(data) {

        drawRecordsLineCharts(data,
                              document.getElementById('chart_rec_temperature'),
                              document.getElementById('chart_rec_pressure'),
                              document.getElementById('chart_rec_humidity'),
                              document.getElementById('chart_rainfall'),
                              document.getElementById('chart_rec_wind_speed'));
    });
}

drawCharts();