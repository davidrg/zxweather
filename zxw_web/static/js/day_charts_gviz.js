/** Code to draw all day-level line charts using the Google Visualisation API.
 *
 * User: David Goodwin
 * Date: 21/06/12
 * Time: 10:24 PM
 */

function drawAllLineCharts(data,
                           tdp_element,
                           awc_element,
                           humidity_element,
                           pressure_element,
                           wind_speed_element) {

    // Temperature and Dewpoint only (columns 1 and 2)
    var temperature_tdp = new google.visualization.DataView(data);
    temperature_tdp.hideColumns([3,4,5,6,7,8]);

    var temperature_tdp_options = {
        title: 'Temperature and Dew Point (°C)',
        legend: {position: 'bottom'}
    };
    var temperature_tdp_chart = new google.visualization.LineChart(tdp_element);
    temperature_tdp_chart.draw(temperature_tdp, temperature_tdp_options);

    // Apparent Temperature and Wind Chill only (columns 3 and 4)
    var temperature_awc = new google.visualization.DataView(data);
    temperature_awc.hideColumns([1,2,5,6,7,8]);

    var temperature_awc_options = {
        title: 'Apparent Temperature and Wind Chill (°C)',
        legend: {position: 'bottom'}
    };
    var temperature_awc_chart = new google.visualization.LineChart(awc_element);
    temperature_awc_chart.draw(temperature_awc, temperature_awc_options);

    // Absolute Pressure only (column 6)
    var pressure = new google.visualization.DataView(data);
    pressure.hideColumns([1,2,3,4,5,7,8]);

    var pressure_options = {
        title: 'Absolute Pressure (hPa)',
        legend: {position: 'none'},
        vAxis: {format: '####'}
    };
    var pressure_chart = new google.visualization.LineChart(pressure_element);
    pressure_chart.draw(pressure, pressure_options);

    // Humidity only (column 5)
    var humidity = new google.visualization.DataView(data);
    humidity.hideColumns([1,2,3,4,6,7,8]);

    var humidity_options = {
        title: 'Humidity (%)',
        legend: {position: 'none'}
    };
    var humidity_chart = new google.visualization.LineChart(humidity_element);
    humidity_chart.draw(humidity, humidity_options);

    // Wind speed only (columns 7 and 8)
    var wind_speed = new google.visualization.DataView(data);
    wind_speed.hideColumns([1,2,3,4,5,6]);

    var wind_speed_options = {
        title: 'Wind Speed (m/s)',
        legend: {position: 'bottom'}

    };
    var wind_speed_chart = new google.visualization.LineChart(wind_speed_element);
    wind_speed_chart.draw(wind_speed, wind_speed_options);
}

// Loads all daily charts.
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

        drawAllLineCharts(sampledata,
                          document.getElementById('chart_temperature_tdp_div'),
                          document.getElementById('chart_temperature_awc_div'),
                          document.getElementById('chart_humidity_div'),
                          document.getElementById('chart_pressure_div'),
                          document.getElementById('chart_wind_speed_div')
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
        // This contains all samples for the day. Included columns are
        // 0: timestamp
        // 1: temperature
        // 2: dewpoint
        // 3: apparenttemp
        // 4: windchill
        // 5: humidity
        // 6: abspressure
        var rainfall_data = new google.visualization.DataTable(data);

        // Do some formatting
        var rainfallFormatter = new google.visualization.NumberFormat(
            { pattern: '##.# mm'});
        rainfallFormatter.format(rainfall_data,1);

        var dateFormatter = new google.visualization.DateFormat(
            { pattern: 'HH'});
        dateFormatter.format(rainfall_data,0);

        var dataView = new google.visualization.DataView(rainfall_data);
        dataView.setColumns([{calc: function(data, row) {
            return data.getFormattedValue(row, 0); }, type:'string'}, 1]);

        var options = {
            title: 'Hourly Rainfall (mm)',
            legend: {position: 'none'},
            isStacked: true,
            focusTarget: 'datum'
        };
        var rainfall_chart = new google.visualization.ColumnChart(
            document.getElementById('chart_hourly_rainfall_div'));
        rainfall_chart.draw(dataView, options);


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


// Loads all 7day charts
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

        drawAllLineCharts(sampledata,
                          document.getElementById('chart_7_temperature_tdp_div'),
                          document.getElementById('chart_7_temperature_awc_div'),
                          document.getElementById('chart_7_humidity_div'),
                          document.getElementById('chart_7_pressure_div'),
                          document.getElementById('chart_7_wind_speed_div')
        );

        samples_7_loading = false;

        // Turn the refresh button back on if we're finished loading
        if (!samples_7_loading && !rainfall_7_loading)
            $("#btn_7day_refresh").button('reset');
    }).error(function() {
                 $("#7day-charts").hide();
                 $("#lc7_refresh_failed").show();
             });

    /***************************************************************
     * Fetch the 7 day hourly rainfall and chart it
     */
    $.getJSON(rainfall_7day_url, function(data) {
        // This contains all samples for the day. Included columns are
        // 0: timestamp
        // 1: temperature
        // 2: dewpoint
        // 3: apparenttemp
        // 4: windchill
        // 5: humidity
        // 6: abspressure
        var rainfall_data = new google.visualization.DataTable(data);

        // Do some formatting
        var rainfallFormatter = new google.visualization.NumberFormat(
            { pattern: '##.# mm'});
        rainfallFormatter.format(rainfall_data,1);

        var dateFormatter = new google.visualization.DateFormat(
            { pattern: 'HH:00 dd-MMM-yyyy'});
        dateFormatter.format(rainfall_data,0);

        var dataView = new google.visualization.DataView(rainfall_data);
        dataView.setColumns([{calc: function(data, row) {
            return data.getFormattedValue(row, 0); }, type:'string'}, 1]);

        var options = {
            title: 'Hourly Rainfall (mm)',
            legend: {position: 'none'},
            isStacked: true,
            focusTarget: 'datum'
        };
        var rainfall_chart = new google.visualization.ColumnChart(
            document.getElementById('chart_7_hourly_rainfall_div'));
        rainfall_chart.draw(dataView, options);

        rainfall_7_loading = false;

        // Turn the refresh button back on if we're finished loading
        if (!samples_7_loading && !rainfall_7_loading)
            $("#btn_7day_refresh").button('reset');
    });
}

if (typeof auto_plot === 'undefined')
  var auto_plot = true;

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
    $('#lc7_obsolete_browser').hide();
    $('#lc_obsolete_browser').hide();
    drawCharts();
    return true;
}