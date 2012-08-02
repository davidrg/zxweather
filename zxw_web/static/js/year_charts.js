/**
 * Draws charts for year-level pages.
 * User: David Goodwin
 * Date: 24/06/12
 * Time: 2:21 PM
 */

google.load("visualization", "1", {packages:["corechart"]});
google.setOnLoadCallback(drawTable);
function drawTable() {
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
    });
}