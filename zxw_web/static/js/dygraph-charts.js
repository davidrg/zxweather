/**
 * Utility functions for working with Dygraphs.
 *
 * User: David Goodwin
 * Date: 2/08/12
 * Time: 9:46 PM
 */

/** Converts date strings to date/time objects.
 *
 * @param data Data to process.
 */
function convertToDates(data) {
    for(var i=0;i<data.length;i++) {
        data[i][0] = new Date(data[i][0]);
    }
    return data;
}

/** Selects a set of columns returning them.
 *
 * @param data Input data
 * @param columns List of column numbers to select.
 * @return {Array}
 */
function selectColumns(data, columns) {
    var new_data = [];
    for (var i=0;i<data.length;i++) {
        var row = [];
        for (var j=0;j<columns.length;j++)
            row.push(data[i][columns[j]]);
        new_data.push(row);
    }
    return new_data;
}

/* Some global settings */
var enable_animated_zooms = true;
var strokeWidth = 1.25;

/* Formatting functions */
var temperatureFormatter = function(y) {
    return y.toFixed(1) + '°C';
};
var humidityFormatter = function(y) {
    return y + '%';
};
var pressureFormatter = function(y) {
    return y.toFixed(1) + ' hPa';
};
var windSpeedFormatter = function(y) {
    return y.toFixed(1) + ' m/s';
};
var rainfallFormatter = function(y) {
    return y.toFixed(1) + 'mm';
};
var dateFormatter = function(y) {
    return new Date(y).strftime('%Y/%m/%d');
};

/** Draws all sample line charts on the day and month-level pages including
 * the station overview page.
 *
 * @param jsondata JSON data (*NOT* the DataTable stuff)
 * @param tdp_element Temperature & Dew Point element
 * @param tdp_key The element to render the key/labels in
 * @param awc_element Apparent Temperature & Wind Chill element
 * @param awc_key The element to render the key/labels in
 * @param humidity_element Humidity element
 * @param humidity_key The element to render the key/labels in
 * @param pressure_element Absolute Pressure element
 * @param pressure_key The element to render the key/labels in
 * @param wind_speed_element Wind Speed element.
 * @param wind_speed_key The element to render the key/labels in
 */
function drawSampleLineCharts(jsondata,
                              tdp_element,
                              tdp_key,
                              awc_element,
                              awc_key,
                              humidity_element,
                              humidity_key,
                              pressure_element,
                              pressure_key,
                              wind_speed_element,
                              wind_speed_key) {


    var labels = jsondata['labels'];
    var data = jsondata['data'];

    /* Make sure the first column is Date objects, not strings */
    data = convertToDates(data);

    /* Split out the data:
     *  0 - Timestamp
     *  1 - Temperature
     *  2 - Dew Point
     *  3 - Apparent Temperature
     *  4 - Wind Chill
     *  5 - Humidity
     *  6 - Absolute Pressure
     *  7 - Average Wind Speed
     *  8 - Gust Wind Speed
     */
    var tdp_data = selectColumns(data, [0,1,2]);
    var awc_data = selectColumns(data, [0,3,4]);
    var humidity_data = selectColumns(data, [0,5]);
    var pressure_data = selectColumns(data, [0,6]);
    var wind_speed_data = selectColumns(data, [0,7,8]);

    /* And the labels */
    var tdp_labels = [labels[0],labels[1],labels[2]];
    var awc_labels = [labels[0],labels[3],labels[4]];
    var humidity_labels = [labels[0],labels[5]];
    var pressure_labels = [labels[0],labels[6]];
    var wind_speed_labels = [labels[0],labels[7],labels[8]];

    /* Now chart it all */
    var tdp_chart = new Dygraph(
        tdp_element,
        tdp_data,
        {
            labels: tdp_labels,
            labelsDiv: tdp_key,
            animatedZooms: enable_animated_zooms,
            strokeWidth: strokeWidth,
            title: 'Temperature and Dew Point (°C)',
            axes: {
                y: {
                    valueFormatter: temperatureFormatter
                }
            },
            legend: 'always'
        });
    var awc_chart = new Dygraph(
        awc_element,
        awc_data,
        {
            labels: awc_labels,
            labelsDiv: awc_key,
            animatedZooms: enable_animated_zooms,
            strokeWidth: strokeWidth,
            title: 'Apparent Temperature and Wind Chill (°C)',
            axes: {
                y: {
                    valueFormatter: temperatureFormatter
                }
            },
            legend: 'always'
        });
    var humidity_chart = new Dygraph(
        humidity_element,
        humidity_data,
        {
            labels: humidity_labels,
            labelsDiv: humidity_key,
            animatedZooms: enable_animated_zooms,
            strokeWidth: strokeWidth,
            title: 'Humidity (%)',
            axes: {
                y: {
                    valueFormatter: humidityFormatter
                }
            },
            legend: 'always'
        });
    var pressure_chart = new Dygraph(
        pressure_element,
        pressure_data,
        {
            labels: pressure_labels,
            labelsDiv: pressure_key,
            animatedZooms: enable_animated_zooms,
            strokeWidth: strokeWidth,
            title: 'Pressure (hPa)',
            axes: {
                y: {
                    valueFormatter: pressureFormatter
                }
            },
            legend: 'always'
        });
    var wind_speed_chart = new Dygraph(
        wind_speed_element,
        wind_speed_data,
        {
            labels: wind_speed_labels,
            labelsDiv: wind_speed_key,
            animatedZooms: enable_animated_zooms,
            strokeWidth: strokeWidth,
            title: 'Wind Speed (m/s)',
            axes: {
                y: {
                   valueFormatter: windSpeedFormatter
                }
            },
            legend: 'always'
        });
}

/** Draws the daily records line charts used on the year- and month-level
 * pages.
 *
 * @param jsondata JSON data to plot
 * @param temperature_element Element to draw the temperature chart in
 * @param temperature_key Element to draw the key/labels in
 * @param pressure_element Element to draw the pressure chart in
 * @param pressure_key Element to draw the key/labels in
 * @param humidity_element Element to draw the humidity chart in
 * @param humidity_key Element to draw the key/labels in
 * @param rainfall_element Element to draw the rainfall chart in
 * @param rainfall_key Element to draw the key/labels in
 * @param wind_speed_element Element to draw the wind speed chart in
 * @param wind_speed_key Element to draw the key/labels in
 */
function drawRecordsLineCharts(jsondata,
                               temperature_element,
                               temperature_key,
                               pressure_element,
                               pressure_key,
                               humidity_element,
                               humidity_key,
                               rainfall_element,
                               rainfall_key,
                               wind_speed_element,
                               wind_speed_key) {

    var labels = jsondata['labels'];
    var data = jsondata['data'];

    /* Make sure the first column is Date objects, not strings */
    data = convertToDates(data);

    /* Split out the data
     * 0 - Timestamp
     * 1 - Max Temp
     * 2 - Min Temp
     * 3 - Max Humidity
     * 4 - Min Humidity
     * 5 - Max Pressure
     * 6 - Min Pressure
     * 7 - Rainfall
     * 8 - Max average wind speed
     * 9 - Min average wind speed
     */
    var temp_data = selectColumns(data, [0,1,2]);
    var humidity_data = selectColumns(data, [0,3,4]);
    var pressure_data = selectColumns(data, [0,5,6]);
    var rainfall_data = selectColumns(data, [0,7]);
    var wind_speed_data = selectColumns(data, [0,8,9]);

    /* And the labels */
    var temp_labels = [labels[0],labels[1],labels[2]];
    var humidity_labels = [labels[0],labels[3],labels[4]];
    var pressure_labels = [labels[0],labels[5],labels[6]];
    var rainfall_labels = [labels[0],labels[7]];
    var wind_speed_labels = [labels[0],labels[8],labels[9]];

    /* Now chart everything */
    var temp_chart = new Dygraph(
        temperature_element,
        temp_data,
        {
            labels: temp_labels,
            labelsDiv: temperature_key,
            animatedZooms: enable_animated_zooms,
            strokeWidth: strokeWidth,
            title: "Temperature (°C)",
            axes: {
                y: {
                    valueFormatter: temperatureFormatter
                },
                x: {
                    valueFormatter: dateFormatter
                }
            },
            legend: 'always'
        });

    var humidity_chart = new Dygraph(
        humidity_element,
        humidity_data,
        {
            labels: humidity_labels,
            labelsDiv: humidity_key,
            animatedZooms: enable_animated_zooms,
            strokeWidth: strokeWidth,
            title: "Humidity (%)",
            axes: {
                y: {
                    valueFormatter: humidityFormatter
                },
                x: {
                    valueFormatter: dateFormatter
                }
            },
            legend: 'always'
        });

    var pressure_chart = new Dygraph(
        pressure_element,
        pressure_data,
        {
            labels: pressure_labels,
            labelsDiv: pressure_key,
            animatedZooms: enable_animated_zooms,
            strokeWidth: strokeWidth,
            title: "Absolute Pressure (hPa)",
            axes: {
                y: {
                    valueFormatter: pressureFormatter
                },
                x: {
                    valueFormatter: dateFormatter
                }
            },
            legend: 'always'
        });

    var rainfall_chart = new Dygraph(
        rainfall_element,
        rainfall_data,
        {
            labels: rainfall_labels,
            labelsDiv: rainfall_key,
            animatedZooms: enable_animated_zooms,
            strokeWidth: strokeWidth,
            title: "Total Rainfall (mm)",
            axes: {
                y: {
                    valueFormatter: rainfallFormatter
                },
                x: {
                    valueFormatter: dateFormatter
                }
            },
            legend: 'always'
        });

    var wind_speed_chart = new Dygraph(
        wind_speed_element,
        wind_speed_data,
        {
            labels: wind_speed_labels,
            labelsDiv: wind_speed_key,
            animatedZooms: enable_animated_zooms,
            strokeWidth: strokeWidth,
            title: "Wind Speed (m/s)",
            axes: {
                y: {
                    valueFormatter: windSpeedFormatter
                },
                x: {
                    valueFormatter: dateFormatter
                }
            },
            legend: 'always'
        });
}
