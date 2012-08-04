/** Indoor day-level charts implemented using Dygraphs.
 *
 * User: David Goodwin
 * Date: 4/08/12
 * Time: 5:12 PM
 */

function drawLineCharts(jsondata, temperature_element, humidity_element) {
    var labels = jsondata['labels'];
    var data = jsondata['data'];

    /* Make sure the first column is Date objects, not strings */
    data = convertToDates(data);

    /* Split out the data:
     * 0 - timestamp
     * 1 - temperature
     * 2 - humidity
     */
    var temperature_data = selectColumns(data, [0, 1]);
    var humidity_data = selectColumns(data, [0,2]);

    /* And the labels */
    var temperature_labels = [labels[0],labels[1]];
    var humidity_labels = [labels[0],labels[2]];

    /* Plot the charts */
    var temperature_chart = new Dygraph(
        temperature_element,
        temperature_data,
        {
            labels: temperature_labels,
            animatedZooms: enable_animated_zooms,
            strokeWidth: strokeWidth,
            title: 'Indoor Temperature (°C)',
            axes: {
                y: {
                    valueFormatter: temperatureFormatter
                }
            },
            labelsDivStyles: {
                'text-align': 'right',
                'background': 'none'
            }
        }
    );

    var humidity_chart = new Dygraph(
        humidity_element,
        humidity_data,
        {
            labels: humidity_labels,
            animatedZooms: enable_animated_zooms,
            strokeWidth: strokeWidth,
            title: 'Indoor Humidity (%)',
            axes: {
                y: {
                    valueFormatter: humidityFormatter
                }
            },
            labelsDivStyles: {
                'text-align': 'right',
                'background': 'none'
            }
        }
    );
}

function drawCharts() {
    $.getJSON(samples_url, function(data) {
        drawLineCharts(data,
                       document.getElementById('chart_temperature_div'),
                       document.getElementById('chart_humidity_div'));
    });

    $.getJSON(samples_7day_url, function(data) {
        drawLineCharts(data,
                       document.getElementById('chart_7_temperature_div'),
                       document.getElementById('chart_7_humidity_div'));
    });
}

drawCharts();