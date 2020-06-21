/** Year charts implemented with the Google Visualisation API.
 *
 * User: David Goodwin
 * Date: 2/8/2012
 * Time: 6:53 PM
 */

function drawRecordsLineCharts(record_data,
                               temperature_element,
                               pressure_element,
                               humidity_element,
                               rainfall_element,
                               wind_speed_element) {
    // Temperature
    var temperature = new google.visualization.DataView(record_data);
    temperature.hideColumns([3,4,5,6,7,8,9]);

    var temperature_options = {
        title: 'Temperature (°C)',
        legend: {position: 'bottom'}
    };
    var temperature_chart = new google.visualization.LineChart(
        temperature_element);
    temperature_chart.draw(temperature, temperature_options);

    // Humidity
    var humidity = new google.visualization.DataView(record_data);
    humidity.hideColumns([1,2,5,6,7,8,9]);

    var humidity_options = {
        title: 'Humidity (%)',
        legend: {position: 'bottom'}
    };
    var humidity_chart = new google.visualization.LineChart(
        humidity_element);
    humidity_chart.draw(humidity, humidity_options);

    // Pressure
    var pressure = new google.visualization.DataView(record_data);
    pressure.hideColumns([1,2,3,4,7,8,9]);

    var pressure_options = {
        title: 'Absolute Pressure (hPa)',
        legend: {position: 'bottom'}
    };
    var pressure_chart = new google.visualization.LineChart(
        pressure_element);
    pressure_chart.draw(pressure, pressure_options);

    // Rainfall
    var rainfall = new google.visualization.DataView(record_data);
    rainfall.hideColumns([1,2,3,4,5,6,8,9]);

    var rainfall_options = {
        title: 'Total Rainfall (mm)',
        legend: {position: 'none'},
        vAxis: {format: '##.#'}
    };
    var rainfall_chart = new google.visualization.LineChart(
        rainfall_element);
    rainfall_chart.draw(rainfall, rainfall_options);

    // Wind speed
    var wind_speed = new google.visualization.DataView(record_data);
    wind_speed.hideColumns([1,2,3,4,5,6,7]);

    var wind_speed_options = {
        title: 'Wind Speed (m/s)',
        legend: {position: 'bottom'}
    };
    var wind_speed_chart = new google.visualization.LineChart(
        wind_speed_element);
    wind_speed_chart.draw(wind_speed, wind_speed_options);
}

function drawCharts() {
    $.getJSON(daily_records_url, function(data) {
        var record_data = new google.visualization.DataTable(data);

        // This contains all samples for the day. Included columns are
        // 0: timestamp
        // 1: max temp
        // 2: min temp
        // 3: max humidity
        // 4: min humidity
        // 5: max pressure
        // 6: min pressure
        // 7: rainfall
        // 8: max average wind speed
        // 9: max gust wind speed


        // Do some formatting
        var temperatureFormatter = new google.visualization.NumberFormat(
            { pattern: '##.# °C'});
        temperatureFormatter.format(record_data,1);
        temperatureFormatter.format(record_data,2);

        var humidityFormatter = new google.visualization.NumberFormat(
            { pattern: '##'});
        humidityFormatter.format(record_data,3);
        humidityFormatter.format(record_data,4);

        var pressureFormatter = new google.visualization.NumberFormat(
            { pattern: '####.# hPa'});
        pressureFormatter.format(record_data,5);
        pressureFormatter.format(record_data,6);

        var rainFormatter = new google.visualization.NumberFormat(
            {pattern: '##.# mm'});
        rainFormatter.format(record_data,7);

        var windSpeedFormatter = new google.visualization.NumberFormat(
            { pattern: '##.# m/s'});
        windSpeedFormatter.format(record_data,8);
        windSpeedFormatter.format(record_data,9);

        drawRecordsLineCharts(record_data,
                              document.getElementById('chart_rec_temperature'),
                              document.getElementById('chart_rec_pressure'),
                              document.getElementById('chart_rec_humidity'),
                              document.getElementById('chart_rainfall'),
                              document.getElementById('chart_rec_wind_speed'));
    }).error(function() {
            $("#records_charts").hide();
            $("#lcr_refresh_failed").show();
        });
}

if (typeof auto_plot === 'undefined')
    var auto_plot = true;

//google.load("visualization", "1", {packages:["corechart"]});
google.charts.load('41', {packages: ['corechart']});
if (auto_plot)
    google.charts.setOnLoadCallback(drawCharts);
else {
    $("#records_charts").hide();
    $('#lcr_obsolete_browser').show();
}

// This is called by the 'Enable Charts' button on the IE8 performance alert.
function ie8_enable_charts() {
    $('#ie8_enable_charts').attr('disabled','');
    $("#records_charts").show();
    $('#lcr_obsolete_browser').hide();
    drawCharts();
    return true;
}


