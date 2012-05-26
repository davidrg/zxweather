/** JavaScript for day charts
 * User: David Goodwin
 * Date: 19/05/12
 * Time: 11:34 PM
 */


function drawAllCharts(data,
                       tdp_element,
                       awc_element,
                       humidity_element,
                       pressure_element) {

    // Temperature and Dewpoint only
    var temperature_tdp = new google.visualization.DataView(data);
    temperature_tdp.hideColumns([3,4,5,6]);

    var temperature_tdp_options = {
        title: 'Temperature and Dew Point (°C)',
        legend: {position: 'bottom'}
    };
    var temperature_tdp_chart = new google.visualization.LineChart(tdp_element);
    temperature_tdp_chart.draw(temperature_tdp, temperature_tdp_options);

    // Apparent Temperature and Wind Chill only
    var temperature_awc = new google.visualization.DataView(data);
    temperature_awc.hideColumns([1,2,5,6]);

    var temperature_awc_options = {
        title: 'Apparent Temperature and Wind Chill (°C)',
        legend: {position: 'bottom'}
    };
    var temperature_awc_chart = new google.visualization.LineChart(awc_element);
    temperature_awc_chart.draw(temperature_awc, temperature_awc_options);

    // Absolute Pressure only
    var pressure = new google.visualization.DataView(data);
    pressure.hideColumns([1,2,3,4,5]);

    var pressure_options = {
        title: 'Absolute Pressure (hPa)',
        legend: {position: 'none'},
        vAxis: {format: '####'}
    };
    var pressure_chart = new google.visualization.LineChart(pressure_element);
    pressure_chart.draw(pressure, pressure_options);

    // Humidity only
    var humidity = new google.visualization.DataView(data);
    humidity.hideColumns([1,2,3,4,6]);

    var humidity_options = {
        title: 'Humidity (%)',
        legend: {position: 'none'}
    };
    var humidity_chart = new google.visualization.LineChart(humidity_element);
    humidity_chart.draw(humidity, humidity_options);
}

// Loads all daily charts.
function load_day_charts() {
    $("#day_charts_cont").show();
    $("#lc_refresh_failed").hide();

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

        drawAllCharts(sampledata,
                      document.getElementById('chart_temperature_tdp_div'),
                      document.getElementById('chart_temperature_awc_div'),
                      document.getElementById('chart_humidity_div'),
                      document.getElementById('chart_pressure_div')
        );
    }).error(function() {
                 $("#day_charts_cont").hide();
                 $("#lc_refresh_failed").show();
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
    });
}

function refresh_day_charts() {
    $("#btn_today_refresh").button('loading');

    $("#chart_temperature_tdp_div").empty();
    $("#chart_temperature_awc_div").empty();
    $("#chart_humidity_div").empty();
    $("#chart_pressure_div").empty();
    $("#chart_hourly_rainfall_div").empty();

    if (is_day_page) {
        $("#chart_temperature_tdp_div").html('<img src="../../../../../images/loading.gif" alt="loading"/>');
        $("#chart_temperature_awc_div").html('<img src="../../../../../images/loading.gif" alt="loading"/>');
        $("#chart_humidity_div").html('<img src="../../../../../images/loading.gif" alt="loading"/>');
        $("#chart_pressure_div").html('<img src="../../../../../images/loading.gif" alt="loading"/>');
        $("#chart_hourly_rainfall_div").html('<img src="../../../../../images/loading.gif" alt="loading"/>');
    } else {
        $("#chart_temperature_tdp_div").html('<img src="../../images/loading.gif" alt="loading"/>');
        $("#chart_temperature_awc_div").html('<img src="../../images/loading.gif" alt="loading"/>');
        $("#chart_humidity_div").html('<img src="../../images/loading.gif" alt="loading"/>');
        $("#chart_pressure_div").html('<img src="../../images/loading.gif" alt="loading"/>');
        $("#chart_hourly_rainfall_div").html('<img src="../../images/loading.gif" alt="loading"/>');
    }

    load_day_charts();
    show_hide_rainfall_charts(1); // 1 = 1day chart only

    $("#btn_today_refresh").button('reset');
}

// Loads all 7day charts
function load_7day_charts() {
    $("#7day-charts").show();
    $("#lc7_refresh_failed").hide();
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

        drawAllCharts(sampledata,
                      document.getElementById('chart_7_temperature_tdp_div'),
                      document.getElementById('chart_7_temperature_awc_div'),
                      document.getElementById('chart_7_humidity_div'),
                      document.getElementById('chart_7_pressure_div')
        );
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
    });
}

function refresh_7day_charts() {
    $("#btn_7day_refresh").button('loading');

    $("#chart_7_temperature_tdp_div").empty();
    $("#chart_7_temperature_awc_div").empty();
    $("#chart_7_humidity_div").empty();
    $("#chart_7_pressure_div").empty();
    $("#chart_7_hourly_rainfall_div").empty();

    if (is_day_page) {
        $("#chart_7_temperature_tdp_div").html('<img src="../../../../../images/loading.gif" alt="loading"/>');
        $("#chart_7_temperature_awc_div").html('<img src="../../../../../images/loading.gif" alt="loading"/>');
        $("#chart_7_humidity_div").html('<img src="../../../../../images/loading.gif" alt="loading"/>');
        $("#chart_7_pressure_div").html('<img src="../../../../../images/loading.gif" alt="loading"/>');
        $("#chart_7_hourly_rainfall_div").html('<img src="../../../../../images/loading.gif" alt="loading"/>');
    } else {
        $("#chart_7_temperature_tdp_div").html('<img src="../../images/loading.gif" alt="loading"/>');
        $("#chart_7_temperature_awc_div").html('<img src="../../images/loading.gif" alt="loading"/>');
        $("#chart_7_humidity_div").html('<img src="../../images/loading.gif" alt="loading"/>');
        $("#chart_7_pressure_div").html('<img src="../../images/loading.gif" alt="loading"/>');
        $("#chart_7_hourly_rainfall_div").html('<img src="../../images/loading.gif" alt="loading"/>');
    }

    load_7day_charts();
    show_hide_rainfall_charts(2); // 2 = 7day chart only

    $("#btn_7day_refresh").button('reset');
}

// Checks the server to see if there has been any rainfall for either of the
// charts. If there hasn't they are hidden.
// chart values:  0 - both charts
//                1 - day chart only
//                2 - 7 day chart only
function show_hide_rainfall_charts(chart) {
    $.getJSON(rainfall_totals_url, function(data) {
        if (chart == 0 || chart == 1) {
            if (data['rainfall'] > 0)
                $("#chart_hourly_rainfall_div").show();
            else
                $("#chart_hourly_rainfall_div").hide();
        }

        if (chart == 0 || chart == 2) {
            if (data['7day_rainfall'] > 0)
                $("#chart_7_hourly_rainfall_div").show();
            else
                $("#chart_7_hourly_rainfall_div").hide();
        }
    }).error(function() {
        $("#chart_hourly_rainfall_div").hide();
        $("#chart_7_hourly_rainfall_div").hide();
    });
}

//google.load('visualization', '1', {packages:['table']});
google.load("visualization", "1", {packages:["corechart"]});
google.setOnLoadCallback(drawCharts);
function drawCharts() {
    load_day_charts();
    load_7day_charts();
    show_hide_rainfall_charts(0); // 0 = both charts
}


