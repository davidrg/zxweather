/** Month charts implemented with Dygraphs
 *
 * User: David Goodwin
 * Date: 21/06/12
 * Time: 10:06 PM
 */

function drawRecordsLineCharts(jsondata,
                               temperature_element,
                               pressure_element,
                               humidity_element,
                               rainfall_element,
                               wind_speed_element) {

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

    /* Some settings */
    var labelStyles = {
        'text-align': 'right',
        'background': 'none'
    };

    /* Now chart everything */
    var temp_chart = new Dygraph(
        temperature_element,
        temp_data,
        {
            labels: temp_labels,
            animatedZooms: enable_animated_zooms,
            strokeWidth: strokeWidth,
            title: "Temperature (°C)",
            axes: {
                y: {
                    valueFormatter: temperatureFormatter
                }
            },
            legend: 'always',
            labelsDivStyles: labelStyles
        });

    var humidity_chart = new Dygraph(
        humidity_element,
        humidity_data,
        {
            labels: humidity_labels,
            animatedZooms: enable_animated_zooms,
            strokeWidth: strokeWidth,
            title: "Humidity (%)",
            axes: {
                y: {
                    valueFormatter: humidityFormatter
                }
            },
            legend: 'always',
            labelsDivStyles: labelStyles
        });

    var pressure_chart = new Dygraph(
        pressure_element,
        pressure_data,
        {
            labels: pressure_labels,
            animatedZooms: enable_animated_zooms,
            strokeWidth: strokeWidth,
            title: "Absolute Pressure (hPa)",
            axes: {
                y: {
                    valueFormatter: pressureFormatter
                }
            },
            legend: 'always',
            labelsDivStyles: labelStyles
        });

    var rainfall_chart = new Dygraph(
        rainfall_element,
        rainfall_data,
        {
            labels: rainfall_labels,
            animatedZooms: enable_animated_zooms,
            strokeWidth: strokeWidth,
            title: "Total Rainfall (mm)",
            axes: {
                y: {
                    valueFormatter: rainfallFormatter
                }
            },
            legend: 'always',
            labelsDivStyles: labelStyles
        });

    var wind_speed_chart = new Dygraph(
        wind_speed_element,
        wind_speed_data,
        {
            labels: wind_speed_labels,
            animatedZooms: enable_animated_zooms,
            strokeWidth: strokeWidth,
            title: "Wind Speed (m/s)",
            axes: {
                y: {
                    valueFormatter: windSpeedFormatter
                }
            },
            legend: 'always',
            labelsDivStyles: labelStyles
        });
}

function drawCharts() {
    /***************************************************************
     * Fetch the days samples and draw the 1-day charts
     */
    $.getJSON(samples_url, function(data) {

        drawSampleLineCharts(
            data,
            document.getElementById('chart_temperature_tdp_div'),
            document.getElementById('chart_temperature_awc_div'),
            document.getElementById('chart_humidity_div'),
            document.getElementById('chart_pressure_div'),
            document.getElementById('chart_wind_speed_div')
        );
    });

    $.getJSON(daily_records_url, function(data) {

        drawRecordsLineCharts(
            data,
            document.getElementById('chart_rec_temperature'),
            document.getElementById('chart_rec_pressure'),
            document.getElementById('chart_rec_humidity'),
            document.getElementById('chart_rainfall'),
            document.getElementById('chart_rec_wind_speed')
        );
    });
}

drawCharts();