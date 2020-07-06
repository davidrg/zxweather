/** Month charts implemented with the Google Visualisation API.
 *
 * User: David Goodwin
 * Date: 21/06/12
 * Time: 10:06 PM
 */


function drawSampleLineCharts(data,
                       tdp_element,
                       awc_element,
                       humidity_element,
                       pressure_element,
                       wind_speed_element,
                       solar_radiation_element,
                       uv_index_element) {

    // Temperature and Dewpoint only
    var temperature_tdp = new google.visualization.DataView(data);
    temperature_tdp.hideColumns([3,4,5,6,7,8,9,10]);

    var temperature_tdp_options = {
        title: 'Temperature and Dew Point (°C)',
        legend: {position: 'bottom'}
    };
    var temperature_tdp_chart = new google.visualization.LineChart(tdp_element);
    temperature_tdp_chart.draw(temperature_tdp, temperature_tdp_options);

    // Apparent Temperature and Wind Chill only
    var temperature_awc = new google.visualization.DataView(data);
    temperature_awc.hideColumns([1,2,5,6,7,8,9,10]);

    var temperature_awc_options = {
        title: 'Apparent Temperature and Wind Chill (°C)',
        legend: {position: 'bottom'}
    };
    var temperature_awc_chart = new google.visualization.LineChart(awc_element);
    temperature_awc_chart.draw(temperature_awc, temperature_awc_options);

    // Absolute Pressure only
    var pressure = new google.visualization.DataView(data);
    pressure.hideColumns([1,2,3,4,5,7,8,9,10]);

    var pressure_options = {
        title: 'Absolute Pressure (hPa)',
        legend: {position: 'none'},
        vAxis: {format: '####'}
    };
    var pressure_chart = new google.visualization.LineChart(pressure_element);
    pressure_chart.draw(pressure, pressure_options);

    // Humidity only
    var humidity = new google.visualization.DataView(data);
    humidity.hideColumns([1,2,3,4,6,7,8,9,10]);

    var humidity_options = {
        title: 'Humidity (%)',
        legend: {position: 'none'}
    };
    var humidity_chart = new google.visualization.LineChart(humidity_element);
    humidity_chart.draw(humidity, humidity_options);

    if (hw_type == "DAVIS" && solar_and_uv_available) {
        // Solar Radiation only (column 10)
        var solar_radiation = new google.visualization.DataView(data);
        solar_radiation.hideColumns([1, 2, 3, 4, 5, 6, 7, 8, 9]);

        var solar_radiation_options = {
            title: 'Solar Radiation (W/m\u00B2)',
            legend: {position: 'none'}
        };
        var solar_radiation_chart = new google.visualization.LineChart(solar_radiation_element);
        solar_radiation_chart.draw(solar_radiation, solar_radiation_options);

        // UV Index only (column 9)
        var uv_index = new google.visualization.DataView(data);
        uv_index.hideColumns([1, 2, 3, 4, 5, 6, 7, 8, 10]);

        var uv_index_options = {
            title: 'UV Index',
            legend: {position: 'none'}
        };
        var uv_index_chart = new google.visualization.LineChart(uv_index_element);
        uv_index_chart.draw(uv_index, uv_index_options);
    }

    // Wind speed only (columns 7 and 8)
    var wind_speed = new google.visualization.DataView(data);
    wind_speed.hideColumns([1,2,3,4,5,6,9,10]);

    var wind_speed_options = {
        title: 'Wind Speed (m/s)',
        legend: {position: 'none'}

    };
    var wind_speed_chart = new google.visualization.LineChart(wind_speed_element);
    wind_speed_chart.draw(wind_speed, wind_speed_options);
}

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

        var solarRadiationFormatter = new google.visualization.NumberFormat(
            { pattern: '#### W/m\u00B2'});
        solarRadiationFormatter.format(sampledata,9);

        drawSampleLineCharts(sampledata,
                             document.getElementById('chart_temperature_tdp_div'),
                             document.getElementById('chart_temperature_awc_div'),
                             document.getElementById('chart_humidity_div'),
                             document.getElementById('chart_pressure_div'),
                             document.getElementById('chart_wind_speed_div'),
                             document.getElementById('chart_solar_radiation_div'),
                             document.getElementById('chart_uv_index_div')
        );
    }).error(function() {
            $("#month_charts").hide();
            $("#lc_refresh_failed").show();
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

        drawRecordsLineCharts(
            record_data,
            document.getElementById('chart_rec_temperature'),
            document.getElementById('chart_rec_pressure'),
            document.getElementById('chart_rec_humidity'),
            document.getElementById('chart_rainfall'),
            document.getElementById('chart_rec_wind_speed')
        );
    }).error(function() {
            $("#records_charts").hide();
            $("#lcr_refresh_failed").show();
        });
}

if (typeof auto_plot === 'undefined')
    var auto_plot = true;

google.charts.load('41', {packages: ['corechart']});
if (auto_plot)
    google.charts.setOnLoadCallback(drawCharts);
else {
    $("#month_charts").hide();
    $("#records_charts").hide();
    $('#lcr_obsolete_browser').show();
    $('#lc_obsolete_browser').show();
}

// This is called by the 'Enable Charts' button on the IE8 performance alert.
function ie8_enable_charts() {
    $('#ie8_enable_charts').attr('disabled','');
    $("#month_charts").show();
    $("#records_charts").show();
    $('#lcr_obsolete_browser').hide();
    $('#lc_obsolete_browser').hide();
    drawCharts();
    return true;
}