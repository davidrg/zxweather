/** Day page line charts implemented using the digraph library for line charts.
 *
 * User: David Goodwin
 * Date: 21/06/12
 * Time: 9:02 PM
 */

function drawAllLineCharts(data,
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
    //var temperature_tdp_chart = new google.visualization.LineChart(tdp_element);
    //temperature_tdp_chart.draw(temperature_tdp, temperature_tdp_options);

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


