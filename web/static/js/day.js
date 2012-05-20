/** JavaScript for day charts and other data sources.
 * User: David Goodwin
 * Date: 19/05/12
 * Time: 11:34 PM
 */

var previous_live = null;

function refresh_live_data() {
    $.getJSON(live_url, function (data) {
        $("#cc_refresh_failed").hide();

        // If the live data is over 5 minutes old show a warning instead.
        if (data['age'] > 300) {
            $("#cc_data_age").html(data['age']);
            $("#current_conditions").hide();
            $("#cc_stale").show();
        } else {
            $("#current_conditions").show();
            $("#cc_stale").hide();

            // Current conditions
            var relative_humidity = data['relative_humidity'];
            var temperature = data['temperature'].toFixed(1);
            var apparent_temp = data['apparent_temperature'].toFixed(1);
            var wind_chill = data['wind_chill'].toFixed(1);
            var dew_point = data['dew_point'].toFixed(1);
            var absolute_pressure = data['absolute_pressure'].toFixed(1);
            var average_wind_speed = data['average_wind_speed'].toFixed(1);
            var gust_wind_speed = data['gust_wind_speed'].toFixed(1);

            // Icons
            var rh='', t='', at='', wc='', dp='', ap='';

            if (previous_live != null) {
                function get_ico(current, prev) {
                    if (current > prev)
                        return '<img src="/images/up.png" alt="increase" title="increase"/>&nbsp;';
                    else if (current == prev)
                        return '<img src="/images/no-change.png" alt="no change" title="no change"/>&nbsp;';
                    else
                        return '<img src="/images/down.png" alt="decrease" title="decrease"/>&nbsp;';
                }

                rh = get_ico(relative_humidity, previous_live['relative_humidity']);
                t = get_ico(temperature, previous_live['temperature'].toFixed(1));
                at = get_ico(apparent_temp, previous_live['apparent_temperature'].toFixed(1));
                wc = get_ico(wind_chill, previous_live['wind_chill'].toFixed(1));
                dp = get_ico(dew_point, previous_live['dew_point'].toFixed(1));
                ap = get_ico(absolute_pressure, previous_live['absolute_pressure'].toFixed(1));
            }

            $("#live_relative_humidity").html(rh + relative_humidity + '%');
            $("#live_temperature").html(t + temperature + '°C');
            $("#live_apparent_temperature").html(at + apparent_temp + '°C');
            $("#live_wind_chill").html(wc + wind_chill + '°C');
            $("#live_dew_point").html(dp + dew_point + '°C');
            $("#live_absolute_pressure").html(ap + absolute_pressure + ' hPa');
            $("#live_avg_wind_speed").html(average_wind_speed + ' m/s');
            $("#live_gust_wind_speed").html(gust_wind_speed + ' m/s');
            $("#live_wind_direction").html(data['wind_direction']);
            $("#current_time").html(data['time_stamp']);

        }
        previous_live = data
    }).error(function() {
        $("#cc_refresh_failed").show();
        $("#current_conditions").hide();
        $("#cc_stale").hide();
    });
}
if (live_auto_refresh) {
    refresh_live_data();
}

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
    $("#chart_temperature_tdp_div").html('<img src="/images/loading.gif" alt="loading"/>');

    $("#chart_temperature_awc_div").empty();
    $("#chart_temperature_awc_div").html('<img src="/images/loading.gif" alt="loading"/>');

    $("#chart_humidity_div").empty();
    $("#chart_humidity_div").html('<img src="/images/loading.gif" alt="loading"/>');

    $("#chart_pressure_div").empty();
    $("#chart_pressure_div").html('<img src="/images/loading.gif" alt="loading"/>');

    $("#chart_hourly_rainfall_div").empty();
    $("#chart_hourly_rainfall_div").html('<img src="/images/loading.gif" alt="loading"/>');

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
    $("#chart_7_temperature_tdp_div").html('<img src="/images/loading.gif" alt="loading"/>');

    $("#chart_7_temperature_awc_div").empty();
    $("#chart_7_temperature_awc_div").html('<img src="/images/loading.gif" alt="loading"/>');

    $("#chart_7_humidity_div").empty();
    $("#chart_7_humidity_div").html('<img src="/images/loading.gif" alt="loading"/>');

    $("#chart_7_pressure_div").empty();
    $("#chart_7_pressure_div").html('<img src="/images/loading.gif" alt="loading"/>');

    $("#chart_7_hourly_rainfall_div").empty();
    $("#chart_7_hourly_rainfall_div").html('<img src="/images/loading.gif" alt="loading"/>');

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

function refresh_records() {
    $("#btn_records_refresh").button('loading');
    $.getJSON(records_url, function(data) {

        $("#records_table").show();
        $("#rec_refresh_failed").hide();

        $("#temp_min").html(data['min_temperature'].toFixed(1) + '°C at ' + data['min_temperature_ts']);
        $("#temp_max").html(data['max_temperature'].toFixed(1) + '°C at ' + data['max_temperature_ts']);

        $("#wc_min").html(data['min_wind_chill'].toFixed(1) + '°C at ' + data['min_wind_chill_ts']);
        $("#wc_max").html(data['max_wind_chill'].toFixed(1) + '°C at ' + data['max_wind_chill_ts']);

        $("#at_min").html(data['min_apparent_temperature'].toFixed(1) + '°C at ' + data['min_apparent_temperature_ts']);
        $("#at_max").html(data['max_apparent_temperature'].toFixed(1) + '°C at ' + data['max_apparent_temperature_ts']);

        $("#dp_min").html(data['min_dew_point'].toFixed(1) + '°C at ' + data['min_dew_point_ts']);
        $("#dp_max").html(data['max_dew_point'].toFixed(1) + '°C at ' + data['max_dew_point_ts']);

        $("#ap_min").html(data['min_absolute_pressure'].toFixed(1) + ' hPa at ' + data['min_absolute_pressure_ts']);
        $("#ap_max").html(data['max_absolute_pressure'].toFixed(1) + ' hPa at ' + data['max_absolute_pressure_ts']);

        $("#rh_min").html(data['min_humidity'] + '% at ' + data['min_humidity_ts']);
        $("#rh_max").html(data['max_humidity'] + '% at ' + data['max_humidity_ts']);

        $("#gws").html(data['max_gust_wind_speed'].toFixed(1) + ' m/s at ' + data['max_gust_wind_speed_ts']);
        $("#aws").html(data['max_average_wind_speed'].toFixed(1) + ' m/s at ' + data['max_average_wind_speed_ts']);

        $("#tot_rainfall").html(data['total_rainfall'].toFixed(1));
        $("#btn_records_refresh").button('reset');
    }).error(function() {
        $("#records_table").hide();
        $("#rec_refresh_failed").show();
        $("#btn_records_refresh").button('reset');
    });
}

if (live_auto_refresh) {
    // Refresh live data every 48 seconds.
    window.setInterval(function(){
        refresh_live_data();
    }, 48000);
}