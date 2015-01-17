/** Wireless station reception for the last 24 hours implemented using Dygraphs
 *
 * Created by david on 17/01/2015.
 */

var receptionFormatter = function(y) {
    return y + '%';
};

function drawLineCharts(jsondata, element, key) {
    var labels = jsondata['labels'];
    var data = jsondata['data'];

    /* Make sure the first column is Date objects, not strings */
    data = convertToDates(data);


    /* Split out the data:
     * 0 - timestamp
     * 1 - reception
     */
    var reception_data = selectColumns(data, [0, 1]);

    var reception_labels = [labels[0],labels[1]];

    /* Plot the charts */
    var reception_chart = new Dygraph(
        element,
        reception_data,
        {
            labels: reception_labels,
            labelsDiv: key,
            animatedZooms: enable_animated_zooms,
            strokeWidth: strokeWidth,
            title: 'Reception',
            axes: {
                y: {
                    valueFormatter: receptionFormatter
                }
            },
            legend: 'always'
        }
    );
}

function drawCharts() {
    $.getJSON(reception_url, function(data) {
        drawLineCharts(data,
            document.getElementById('chart_reception_div'),
            document.getElementById('chart_reception_key')
        );
    });

    $.getJSON(reception_7day_url, function(data) {
        drawLineCharts(data,
            document.getElementById('chart_7_reception_div'),
            document.getElementById('chart_7_reception_key')
        );
    });
}

if (Modernizr.canvas) {
    drawCharts();
} else {
    var msg = '<strong>Unsupported browser!</strong> These charts can not be drawn because your browser does not support the required features.';
    $('#lc7_msg').html(msg);
    $('#lc_msg').html(msg);

    if (typeof ie8 === 'undefined')
        var ie8 = false;

    // Only show the Canvas warning if the user isn't using IE8. For IE8 we have
    // a different warning message as the Alternate interface has some performance
    // issues there too.
    if (!ie8)
        $('#canvas_missing').show();

    $('#lc7_obsolete_browser').show();
    $('#lc_obsolete_browser').show();
    $('#7day-charts').hide();
    $('#7-day-charts').hide();
}