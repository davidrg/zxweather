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