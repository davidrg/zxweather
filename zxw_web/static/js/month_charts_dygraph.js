/** Month charts implemented with the Goolge Visualisation API.
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
                       wind_speed_element) {

    // Temperature and Dewpoint only
    var temperature_tdp = new google.visualization.DataView(data);
    temperature_tdp.hideColumns([3,4,5,6,7,8]);

    var temperature_tdp_options = {
        title: 'Temperature and Dew Point (°C)',
        legend: {position: 'bottom'}
    };
    var temperature_tdp_chart = new Dygraph.GVizChart(tdp_element);
    temperature_tdp_chart.draw(temperature_tdp, temperature_tdp_options);

    // Apparent Temperature and Wind Chill only
    var temperature_awc = new google.visualization.DataView(data);
    temperature_awc.hideColumns([1,2,5,6,7,8]);

    var temperature_awc_options = {
        title: 'Apparent Temperature and Wind Chill (°C)',
        legend: {position: 'bottom'}
    };
    var temperature_awc_chart = new Dygraph.GVizChart(awc_element);
    temperature_awc_chart.draw(temperature_awc, temperature_awc_options);

    // Absolute Pressure only
    var pressure = new google.visualization.DataView(data);
    pressure.hideColumns([1,2,3,4,5,7,8]);

    var pressure_options = {
        title: 'Absolute Pressure (hPa)',
        legend: {position: 'none'},
        vAxis: {format: '####'}
    };
    var pressure_chart = new Dygraph.GVizChart(pressure_element);
    pressure_chart.draw(pressure, pressure_options);

    // Humidity only
    var humidity = new google.visualization.DataView(data);
    humidity.hideColumns([1,2,3,4,6,7,8]);

    var humidity_options = {
        title: 'Humidity (%)',
        legend: {position: 'none'}
    };
    var humidity_chart = new Dygraph.GVizChart(humidity_element);
    humidity_chart.draw(humidity, humidity_options);

    // Wind speed only (columns 7 and 8)
    var wind_speed = new google.visualization.DataView(data);
    wind_speed.hideColumns([1,2,3,4,5,6]);

    var wind_speed_options = {
        title: 'Wind Speed (m/s)',
        legend: {position: 'bottom'}

    };
    var wind_speed_chart = new Dygraph.GVizChart(wind_speed_element);
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
    var temperature_chart = new Dygraph.GVizChart(temperature_element);
    temperature_chart.draw(temperature, temperature_options);

    // Humidity
    var humidity = new google.visualization.DataView(record_data);
    humidity.hideColumns([1,2,5,6,7,8,9]);

    var humidity_options = {
        title: 'Humidity (%)',
        legend: {position: 'bottom'}
    };
    var humidity_chart = new Dygraph.GVizChart(humidity_element);
    humidity_chart.draw(humidity, humidity_options);

    // Pressure
    var pressure = new google.visualization.DataView(record_data);
    pressure.hideColumns([1,2,3,4,7,8,9]);

    var pressure_options = {
        title: 'Absolute Pressure (hPa)',
        legend: {position: 'bottom'}
    };
    var pressure_chart = new Dygraph.GVizChart(pressure_element);
    pressure_chart.draw(pressure, pressure_options);

    // Rainfall
    var rainfall = new google.visualization.DataView(record_data);
    rainfall.hideColumns([1,2,3,4,5,6,8,9]);

    var rainfall_options = {
        title: 'Total Rainfall (mm)',
        legend: {position: 'none'},
        vAxis: {format: '##.#'}
    };
    var rainfall_chart = new Dygraph.GVizChart(
        rainfall_element);
    rainfall_chart.draw(rainfall, rainfall_options);

    // Wind speed
    var wind_speed = new google.visualization.DataView(record_data);
    wind_speed.hideColumns([1,2,3,4,5,6,7]);

    var wind_speed_options = {
        title: 'Wind Speed (m/s)',
        legend: {position: 'bottom'}
    };
    var wind_speed_chart = new Dygraph.GVizChart(wind_speed_element);
    wind_speed_chart.draw(wind_speed, wind_speed_options);
}