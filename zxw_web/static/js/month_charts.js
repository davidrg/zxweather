/** Month charts implemented with the Goolge Visualisation API.
 *
 * User: David Goodwin
 * Date: 21/06/12
 * Time: 10:06 PM
 */

//google.load('visualization', '1', {packages:['table']});
google.load("visualization", "1", {packages:["corechart"]});
google.setOnLoadCallback(drawTable);
function drawTable() {
    /***************************************************************
     * Fetch the days samples and draw the 1-day charts
     */
    $.getJSON(samples_url, function(data) {
        // This contains all samples for the day. Included columns are
        // 0: timestamp
        // 1: temperature
        // 2: dewpoint
        // 3: apparenttemp
        // 4: windchill
        // 5: humidity
        // 6: abspressure
        var sampledata = new google.visualization.DataTable(data);

        // Do some formatting
        var temperatureFormatter = new google.visualization.NumberFormat(
            { pattern: '##.# °C'});
        temperatureFormatter.format(sampledata,1);
        temperatureFormatter.format(sampledata,2);
        temperatureFormatter.format(sampledata,3);
        temperatureFormatter.format(sampledata,4);

        var humidityFormatter = new google.visualization.NumberFormat(
            { pattern: '##'});
        humidityFormatter.format(sampledata,5);

        var pressureFormatter = new google.visualization.NumberFormat(
            { pattern: '####.# hPa'});
        pressureFormatter.format(sampledata,6);

        var windSpeedFormatter = new google.visualization.NumberFormat(
            { pattern: '##.# m/s'});
        windSpeedFormatter.format(sampledata,7);
        windSpeedFormatter.format(sampledata,8);

        drawSampleLineCharts(sampledata,
                             document.getElementById('chart_temperature_tdp_div'),
                             document.getElementById('chart_temperature_awc_div'),
                             document.getElementById('chart_humidity_div'),
                             document.getElementById('chart_pressure_div'),
                             document.getElementById('chart_wind_speed_div')
        );
    });

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

        // Temperature
        var temperature = new google.visualization.DataView(record_data);
        temperature.hideColumns([3,4,5,6,7,8,9]);

        var temperature_options = {
            title: 'Temperature (°C)',
            legend: {position: 'bottom'}
        };
        var temperature_chart = new google.visualization.LineChart(
            document.getElementById('chart_rec_temperature'));
        temperature_chart.draw(temperature, temperature_options);

        // Humidity
        var humidity = new google.visualization.DataView(record_data);
        humidity.hideColumns([1,2,5,6,7,8,9]);

        var humidity_options = {
            title: 'Humidity (%)',
            legend: {position: 'bottom'}
        };
        var humidity_chart = new google.visualization.LineChart(
            document.getElementById('chart_rec_humidity'));
        humidity_chart.draw(humidity, humidity_options);

        // Pressure
        var pressure = new google.visualization.DataView(record_data);
        pressure.hideColumns([1,2,3,4,7,8,9]);

        var pressure_options = {
            title: 'Absolute Pressure (hPa)',
            legend: {position: 'bottom'}
        };
        var pressure_chart = new google.visualization.LineChart(
            document.getElementById('chart_rec_pressure'));
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
            document.getElementById('chart_rainfall'));
        rainfall_chart.draw(rainfall, rainfall_options);

        // Wind speed
        var wind_speed = new google.visualization.DataView(record_data);
        wind_speed.hideColumns([1,2,3,4,5,6,7]);

        var wind_speed_options = {
            title: 'Wind Speed (m/s)',
            legend: {position: 'bottom'}
        };
        var wind_speed_chart = new google.visualization.LineChart(
            document.getElementById('chart_rec_wind_speed'));
        wind_speed_chart.draw(wind_speed, wind_speed_options);
    });
}