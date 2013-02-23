/** Indoor day-level charts implemented using the Google Visualisation API.
 *
 * User: David Goodwin
 * Date: 4/08/12
 * Time: 5:05 PM
 */

function drawCharts() {

    function drawAllCharts(data,
                           temperature_element,
                           humidity_element) {

        // Temperature and Dewpoint only
        var temperature = new google.visualization.DataView(data);
        temperature.hideColumns([2]);

        var temperature_options = {
            title: 'Indoor Temperature (°C)',
            legend: {position: 'none'}
        };
        var temperature_chart = new google.visualization.LineChart(temperature_element);
        temperature_chart.draw(temperature, temperature_options);


        // Humidity only
        var humidity = new google.visualization.DataView(data);
        humidity.hideColumns([1]);

        var humidity_options = {
            title: 'Indoor Humidity (%)',
            legend: {position: 'none'}
        };
        var humidity_chart = new google.visualization.LineChart(humidity_element);
        humidity_chart.draw(humidity, humidity_options);
    }

    /***************************************************************
     * Fetch the days samples and draw the 1-day charts
     */
    $.getJSON(samples_url, function(data) {
        // This contains all samples for the day. Included columns are
        // 0: timestamp
        // 1: temperature
        // 2: humidity
        var sampledata = new google.visualization.DataTable(data);

        // Do some formatting
        var temperatureFormatter = new google.visualization.NumberFormat(
            { pattern: '##.# °C'});
        temperatureFormatter.format(sampledata,1);

        var humidityFormatter = new google.visualization.NumberFormat(
            { pattern: '##'});
        humidityFormatter.format(sampledata,2);

        drawAllCharts(sampledata,
                      document.getElementById('chart_temperature_div'),
                      document.getElementById('chart_humidity_div')
        );
    });

    /***************************************************************
     * Fetch samples for the past 7 days and draw the 7-day charts
     */
    $.getJSON(samples_7day_url, function(data) {
        // This contains all samples for the day. Included columns are
        // 0: timestamp
        // 1: temperature
        // 2: humidity
        var sampledata = new google.visualization.DataTable(data);

        // Do some formatting
        var temperatureFormatter = new google.visualization.NumberFormat(
            { pattern: '##.# °C'});
        temperatureFormatter.format(sampledata,1);

        var humidityFormatter = new google.visualization.NumberFormat(
            { pattern: '##'});
        humidityFormatter.format(sampledata,2);


        drawAllCharts(sampledata,
                      document.getElementById('chart_7_temperature_div'),
                      document.getElementById('chart_7_humidity_div')
        );
    });
}

var auto_plot = true;
if ($.browser.msie && ($.browser.version == '8.0' || $.browser.version == '7.0')) {
    // On my i7 box IE8 locks up for a second or two as it tries to draw the
    // charts. For this reason we won't draw them automatically - instead we
    // warn the user and make them click a button to get the charts.
    // When IE8 is in IE7 compatibility mode it reports itself as IE 7 so we
    // check for that too.
    auto_plot = false;
}

google.load("visualization", "1", {packages:["corechart"]});
if (auto_plot)
    google.setOnLoadCallback(drawCharts);
else {
    $("#7day-charts").hide();
    $("#day_charts_cont").hide();
    $('#lc7_obsolete_browser').show();
    $('#lc_obsolete_browser').show();
}

// This is called by the 'Enable Charts' button on the IE8 performance alert.
function ie8_enable_charts() {
    $('#ie8_enable_charts').attr('disabled','');
    $("#7day-charts").show();
    $("#day_charts_cont").show();
    $('#lc7_obsolete_browser').hide();
    $('#lc_obsolete_browser').hide();
    drawCharts();
    return true;
}