/**
 * Utility functions for working with Dygraphs.
 *
 * User: David Goodwin
 * Date: 2/08/12
 * Time: 9:46 PM
 */

/** Converts date strings to date objects.
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

/** Draws all sample line charts on the day and month-level pages including
 * the station overview page.
 *
 * @param jsondata JSON data (*NOT* the DataTable stuff)
 * @param tdp_element Temperature & Dew Point element
 * @param awc_element Apparent Temperature & Wind Chill element
 * @param humidity_element Humidity element
 * @param pressure_element Absolute Pressure element
 * @param wind_speed_element Wind Speed element.
 */
function drawSampleLineCharts(jsondata,
                              tdp_element,
                              awc_element,
                              humidity_element,
                              pressure_element,
                              wind_speed_element) {


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
    var tdp_chart = new Dygraph(tdp_element,
                                tdp_data,
                                {
                                    labels: tdp_labels,
                                    animatedZooms: enable_animated_zooms,
                                    strokeWidth: strokeWidth,
                                    title: 'Temperature and Dew Point (°C)',
                                    axes: {
                                        y: {
                                            valueFormatter: temperatureFormatter
                                        }
                                    },
                                    legend: 'always',
                                    labelsDivStyles: {
                                        'text-align': 'right',
                                        'background': 'none'
                                    }
                                });
    var awc_chart = new Dygraph(awc_element,
                                awc_data,
                                {
                                    labels: awc_labels,
                                    animatedZooms: enable_animated_zooms,
                                    strokeWidth: strokeWidth,
                                    title: 'Apparent Temperature and Wind Chill (°C)',
                                    axes: {
                                        y: {
                                            valueFormatter: temperatureFormatter
                                        }
                                    },
                                    legend: 'always',
                                    labelsDivStyles: {
                                        'text-align': 'right',
                                        'background': 'none'
                                    }
                                });
    var humidity_chart = new Dygraph(humidity_element,
                                     humidity_data,
                                     {
                                         labels: humidity_labels,
                                         animatedZooms: enable_animated_zooms,
                                         strokeWidth: strokeWidth,
                                         title: 'Humidity (%)',
                                         axes: {
                                             y: {
                                                 valueFormatter: humidityFormatter
                                             }
                                         },
                                         legend: 'always',
                                         labelsDivStyles: {
                                             'text-align': 'right',
                                             'background': 'none'
                                         }
                                     });
    var pressure_chart = new Dygraph(pressure_element,
                                     pressure_data,
                                     {
                                         labels: pressure_labels,
                                         animatedZooms: enable_animated_zooms,
                                         strokeWidth: strokeWidth,
                                         title: 'Absolute Pressure (hPa)',
                                         axes: {
                                             y: {
                                                 valueFormatter: pressureFormatter
                                             }
                                         },
                                         legend: 'always',
                                         labelsDivStyles: {
                                             'text-align': 'right',
                                             'background': 'none'
                                         }
                                     });
    var wind_speed_chart = new Dygraph(wind_speed_element,
                                       wind_speed_data,
                                       {
                                           labels: wind_speed_labels,
                                           animatedZooms: enable_animated_zooms,
                                           strokeWidth: strokeWidth,
                                           title: 'Wind Speed (m/s)',
                                           axes: {
                                               y: {
                                                   valueFormatter: windSpeedFormatter
                                               }
                                           },
                                           legend: 'always',
                                           labelsDivStyles: {
                                               'text-align': 'right',
                                               'background': 'none'
                                           }
                                       });
}
