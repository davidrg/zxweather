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
var solarRadiationFormatter = function(y) {
    return y.toFixed(1) + '  W/m&sup2;';
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
 * @param solar_radiation_element Solar Radiation chart element
 * @param solar_radiation_key The element to render the key/labels in
 * @param uv_index_element UV Index chart element
 * @param uv_index_key The element to render the key/labels in
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
                              wind_speed_key,
                              solar_radiation_element,
                              solar_radiation_key,
                              uv_index_element,
                              uv_index_key) {


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
     *  9 - UV Index
     *  10 - Solar Radiation
     */

    var data_set = {
        tdp_data : selectColumns(data, [0,1,2]),
        awc_data : selectColumns(data, [0,3,4]),
        humidity_data : selectColumns(data, [0,5]),
        pressure_data : selectColumns(data, [0,6]),
        wind_speed_data : selectColumns(data, [0,7,8]),
        uv_index_data : null,
        solar_radiation_data : null,
        _graphs: {
            tdp_chart: null,
            awc_chart: null,
            humidity_chart: null,
            pressure_chart: null,
            uv_index_chart: null,
            solar_radiation_chart: null,
            wind_speed_chart: null
        },
        update_graphs: function() {
            this._graphs.tdp_chart.updateOptions({ 'file': this.tdp_data } );
            this._graphs.awc_chart.updateOptions({ 'file': this.awc_data } );
            this._graphs.humidity_chart.updateOptions({ 'file': this.humidity_data } );
            this._graphs.pressure_chart.updateOptions({ 'file': this.pressure_data } );
            if (this._graphs.uv_index_chart != null) {
                this._graphs.uv_index_chart.updateOptions({'file': this.uv_index_data});
            }
            if (this._graphs.solar_radiation_chart != null) {
                this._graphs.solar_radiation_chart.updateOptions({'file': this.solar_radiation_data});
            }
            this._graphs.wind_speed_chart.updateOptions({ 'file': this.wind_speed_data } );
        }
    };

    if (hw_type == "DAVIS" && solar_and_uv_available) {
        data_set.uv_index_data = selectColumns(data, [0, 9]);
        data_set.solar_radiation_data = selectColumns(data, [0, 10]);
    }

    /* And the labels */
    var tdp_labels = [labels[0],labels[1],labels[2]];
    var awc_labels = [labels[0],labels[3],labels[4]];
    var humidity_labels = [labels[0],labels[5]];
    var pressure_labels = [labels[0],labels[6]];
    var wind_speed_labels = [labels[0],labels[7],labels[8]];
    var uv_index_labels = [labels[0], labels[9]];
    var solar_radiation_labels = [labels[0], labels[10]];

    /* Now chart it all */
    data_set._graphs.tdp_chart = new Dygraph(
        tdp_element,
        data_set.tdp_data,
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
    data_set._graphs.awc_chart = new Dygraph(
        awc_element,
        data_set.awc_data,
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
    data_set._graphs.humidity_chart = new Dygraph(
        humidity_element,
        data_set.humidity_data,
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
    data_set._graphs.pressure_chart = new Dygraph(
        pressure_element,
        data_set.pressure_data,
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

    if (data_set.uv_index_data != null) {
        data_set._graphs.uv_index_chart = new Dygraph(
            uv_index_element,
            data_set.uv_index_data,
            {
                labels: uv_index_labels,
                labelsDiv: uv_index_key,
                animatedZooms: enable_animated_zooms,
                strokeWidth: strokeWidth,
                title: 'UV Index',
                legend: 'always'
            }
        )
    }

    if (data_set.solar_radiation_data != null) {
        data_set._graphs.solar_radiation_chart = new Dygraph(
            solar_radiation_element,
            data_set.solar_radiation_data,
            {
                labels: solar_radiation_labels,
                labelsDiv: solar_radiation_key,
                animatedZooms: enable_animated_zooms,
                strokeWidth: strokeWidth,
                title: 'Solar Radiation (W/m&sup2;)',
                axes: {
                    y: {
                       valueFormatter: solarRadiationFormatter
                    }
                },
                legend: 'always'
            }
        )
    }

    data_set._graphs.wind_speed_chart = new Dygraph(
        wind_speed_element,
        data_set.wind_speed_data,
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

    return data_set;
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
            title: "Pressure (hPa)",
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

// Disable touch interaction - it causes usability problems on small screens
// such as mobile phones. May re-enable it sometime in the future if I can
// find a way to only enable it for tablets.
Dygraph.Interaction.endTouch =
    Dygraph.Interaction.moveTouch =
        Dygraph.Interaction.startTouch = function() {};