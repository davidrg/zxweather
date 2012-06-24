/** JavaScript for day charts
 * User: David Goodwin
 * Date: 19/05/12
 * Time: 11:34 PM
 */


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

    $("#chart_temperature_tdp_div").html('<div class="bg_loading"></div>');
    $("#chart_temperature_awc_div").html('<div class="bg_loading"></div>');
    $("#chart_humidity_div").html('<div class="bg_loading"></div>');
    $("#chart_pressure_div").html('<div class="bg_loading"></div>');
    $("#chart_hourly_rainfall_div").html('<div class="bg_loading"></div>');

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

    $("#chart_7_temperature_tdp_div").html('<div class="bg_loading"></div>');
    $("#chart_7_temperature_awc_div").html('<div class="bg_loading"></div>');
    $("#chart_7_humidity_div").html('<div class="bg_loading"></div>');
    $("#chart_7_pressure_div").html('<div class="bg_loading"></div>');
    $("#chart_7_hourly_rainfall_div").html('<div class="bg_loading"></div>');

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


