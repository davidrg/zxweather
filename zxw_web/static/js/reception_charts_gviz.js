/** Wireless station reception for the last 24 hours implemented using the
 * Google Visualisation API.
 *
 * Created by david on 17/01/2015.
 */

function drawLineCharts(data, element) {
    // Temperature and Dewpoint only
    var reception = new google.visualization.DataView(data);

    var options = {
        title: 'Reception (%)',
        legend: {position: 'none'}
    };

    var reception_chart = new google.visualization.LineChart(element);
    reception_chart.draw(reception, options);
}

function drawCharts() {

    $.getJSON(reception_url, function(data) {
        // This contains all samples for the day. Included columns are
        // 0: timestamp
        // 1: reception
        var sampledata = new google.visualization.DataTable(data);

        // Do some formatting
        var receptionFormatter = new google.visualization.NumberFormat(
            { pattern: '##.#'});
        receptionFormatter.format(sampledata,1);

        drawLineCharts(sampledata, document.getElementById('chart_reception_div'));
    });

    $.getJSON(reception_7day_url, function(data) {
        // This contains all samples for the day. Included columns are
        // 0: timestamp
        // 1: reception
        var sampledata = new google.visualization.DataTable(data);

        // Do some formatting
        var receptionFormatter = new google.visualization.NumberFormat(
            { pattern: '##.#'});
        receptionFormatter.format(sampledata,1);

        drawLineCharts(sampledata, document.getElementById('chart_7_reception_div'));
    });
}

if (typeof auto_plot === 'undefined')
    var auto_plot = true;

google.load("visualization", "1", {packages:["corechart"]});
if (auto_plot) {
    google.setOnLoadCallback(drawCharts);
} else {
    $('#lc7_obsolete_browser').show();
    $('#lc_obsolete_browser').show();
    $('#7day-charts').hide();
    $('#7-day-charts').hide();
}

// This is called by the 'Enable Charts' button on the IE8 performance alert.
function ie8_enable_charts() {
    $('#ie8_enable_charts').attr('disabled','');
    $("#7day-charts").show();
    $("#7-day-charts").show();
    $('#lc7_obsolete_browser').hide();
    $('#lc_obsolete_browser').hide();
    drawCharts();
    return true;
}