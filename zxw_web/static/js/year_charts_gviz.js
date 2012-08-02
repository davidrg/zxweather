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