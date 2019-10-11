/****************************************************************************
 * WeatherLink-compatible Leaf Wet Hours report                             *
 ****************************************************************************
 * This script handles calculation of a few summary values plus a bit of    *
 * output formatting. Compared to the WeatherLink report the differences    *
 * are:                                                                     *
 *      - fix typo in WeatherLink report ("Leaf Sensor Nunber")             *
 *      - Fix truncation of low/high temperature to 0dp                     *
 *      - Fix bug where celsius criteria is mangled by conversion to        *
 *        fahrenheit, truncated to 0dp then converted back to celsius       *
 *        resulting in odd output (enter 1.0, get 0.6 out)                  *
 *      - Include sensor names where the user has set one                   *
 *      - Reported day count isn't fixed to "Last 30 days" when most of the *
 *        timespan is in the future                                         *
 ****************************************************************************/

if (!String.prototype.repeat) {
  String.prototype.repeat = function(count) {
    'use strict';
    if (this == null) { // check if `this` is null or undefined
      throw new TypeError('can\'t convert ' + this + ' to object');
    }
    var str = '' + this;
    // To convert string to integer.
    count = +count;
    if (count < 0) {
      throw new RangeError('repeat count must be non-negative');
    }
    if (count == Infinity) {
      throw new RangeError('repeat count must be less than infinity');
    }
    count |= 0; // floors and rounds-down it.
    if (str.length == 0 || count == 0) {
      return '';
    }
    // Ensuring count is a 31-bit integer allows us to heavily optimize the
    // main part. But anyway, most current (August 2014) browsers can't handle
    // strings 1 << 28 chars or longer, so:
    if (str.length * count >= (1 << 28)) {
      throw new RangeError('repeat count must not overflow maximum string size');
    }
    while (count >>= 1) { // shift it by multiple of 2 because this is binary summation of series
       str += str; // binary summation
    }
    str += str.substring(0, str.length * count - str.length);
    return str;
  }
}

if (!String.prototype.padStart) {
   String.prototype.padStart = function padStart(targetLength, padString) {
     targetLength = targetLength >> 0; //truncate if number, or convert non-number to 0;
         padString = String(typeof padString !== 'undefined' ? padString : ' ');
     if (this.length >= targetLength) {
       return String(this);
     } else {
       targetLength = targetLength - this.length;
       if (targetLength > padString.length) {
         padString += padString.repeat(targetLength / padString.length); //append to original to ensure we are longer than needed
       }
       return padString.slice(0, targetLength) + String(this);
     }
   };
 }

function transform_datasets(criteria, datasets) {
    var dataSetIdx;
    for(var i = 0; i < datasets.length; i++) {
        if (datasets[i]["name"] === "data") {
            dataSetIdx = i;
        }
    }
    if (dataSetIdx === undefined) {
        return []; // Couldn't find the input data.
    }

    // Find the columns.
    var column_index = {};
    for (i = 0; i < datasets[dataSetIdx]["column_names"].length; i++) {
        var columnName = datasets[dataSetIdx]["column_names"][i];
        column_index[columnName] = i;
    }

    var idxDate = column_index["date"];
    var idxWetness1 = column_index["wetness_1"];
    var idxWetness2 = column_index["wetness_2"];
    var idxMinTemp = column_index["min_temp"];
    var idxMaxtemp = column_index["max_temp"];

    if (idxDate === undefined) {
        throw new Error("Could not find result column date");
    }

    if (idxWetness1 === undefined) {
        throw new Error("Could not find result column wetness_1");
    }

    if (idxWetness2 === undefined) {
        throw new Error("Could not find result column wetness_2");
    }

    var sensor_names = criteria["sensor_names"];

    var wetness_1_enabled = sensor_names.hasOwnProperty("leaf_wetness_1");
    var wetness_2_enabled = sensor_names.hasOwnProperty("leaf_wetness_2");

    var total_hours_1 = 0;
    var total_hours_2 = 0;

    var hours_list_1 = [];
    var hours_list_2 = [];

    console.log("Processing wetness data...");
    for (i = 0; i < datasets[dataSetIdx]["row_data"].length; i++) {
        var date = moment(datasets[dataSetIdx]["row_data"][i][idxDate]);
        var wetness_1 = parseFloat(datasets[dataSetIdx]["row_data"][i][idxWetness1]);
        var wetness_2 = parseFloat(datasets[dataSetIdx]["row_data"][i][idxWetness2]);

        total_hours_1 += wetness_1;
        total_hours_2 += wetness_2;

        // console.log("row " + i.toString() +
        //     " date " + date.toString() +
        //     " wetness1 " + wetness_1.toString() +
        //     " wetness2 " + wetness_2.toString() +
        //     " total_1 " + total_hours_1.toString() +
        //     " total_2 " + total_hours_2.toString()
        // );

        hours_list_1.push([date.format("DD/MM/YY"), wetness_1.toFixed(1).padStart(4)]);
        hours_list_2.push([date.format("DD/MM/YY"), wetness_2.toFixed(1).padStart(4)]);
    }

    var returned_datasets = [];

    var at_date = moment().format("DD/MM/YY");
    var start_date = moment(criteria["start"]).format("DD/MM/YY");
    var end_date = moment(criteria["end"]).format("DD/MM/YY");
    var low_temp = criteria["low_temp"].toFixed(1);
    var high_temp = criteria["high_temp"].toFixed(1);
    var wetness_threshold = criteria["wetness_threshold"];
    var leaf_sensor_1 = wetness_1_enabled ? "1 (" + sensor_names["leaf_wetness_1"] + ")": "";
    var leaf_sensor_2 = wetness_2_enabled ? "2 (" + sensor_names["leaf_wetness_2"] + ")": "";

    var temp_sensor = criteria["temp_sensor_value"];

    var temp_sensor_name = null;
    if (sensor_names.hasOwnProperty(temp_sensor)) {
        temp_sensor_name = sensor_names[temp_sensor]
    }

    if (temp_sensor === "indoor_temperature")
        temp_sensor = "Inside Temperature";
    else if (temp_sensor === "outdoor_temperature")
        temp_sensor = "Outside Temperature";
    else if (temp_sensor === "leaf_temperature_1")
        temp_sensor = "Leaf Temperature 1";
    else if (temp_sensor === "leaf_temperature_2")
        temp_sensor = "Leaf Temperature 2";
    else if (temp_sensor === "soil_temperature_1")
        temp_sensor = "Soil Temperature 1";
    else if (temp_sensor === "soil_temperature_2")
        temp_sensor = "Soil Temperature 2";
    else if (temp_sensor === "soil_temperature_3")
        temp_sensor = "Soil Temperature 3";
    else if (temp_sensor === "soil_temperature_4")
        temp_sensor = "Soil Temperature 4";
    else if (temp_sensor === "extra_temperature_1")
        temp_sensor = "Temperature 2";
    else if (temp_sensor === "extra_temperature_2")
        temp_sensor = "Temperature 3";
    else if (temp_sensor === "extra_temperature_3")
        temp_sensor =  "Temperature 4";

    if (temp_sensor_name !== null) {
        temp_sensor = temp_sensor + " (" + temp_sensor_name + ")";
    }

    var end_date_d = moment(criteria["end"]);
    if (end_date_d > moment()) {
        // If we're saying "Last x days" we really ought to ensure all x days
        // are in the past, not the future!
        end_date_d = moment();
    }
    var day_count = end_date_d.diff(moment(criteria["start"]), 'days') + 1;
    if (day_count > 60) {
        day_count = end_date_d.diff(end_date_d, 'days') + 1;
        start_date = end_date_d;
    }

    // Insert blanks if the number of rows we got back from the database is
    // less than we were expecting. But only up to 60 rows.
    if (hours_list_1.length < day_count) {
        var blanks = day_count - hours_list_1.length;
        if (blanks > 59) blanks = 59;
        for (i = 0; i < blanks; i++) {
            hours_list_1.push([
                "  ----",
                ""
            ]);
        }
    }

    if (hours_list_2.length < day_count) {
        blanks = day_count - hours_list_2.length;
        if (blanks > 59) blanks = 59;
        for (i = 0; i < blanks; i++) {
            hours_list_2.push([
                "  ----",
                ""
            ]);
        }
    }

    if (wetness_1_enabled) {
        returned_datasets.push({
            "name": "sensor_1_days",
            "column_names": [
                "date",
                "wetness_hours"
            ],
            "row_data": hours_list_1
        });
        returned_datasets.push({
            "name": "sensor_1_summary",
            "column_names": [
                "at_date",
                "leaf_sensor",
                "temp_sensor",
                "total_hours",
                "start_date",
                "end_date",
                "lo_temp",
                "hi_temp",
                "threshold",
                "total_days"
            ],
            "row_data": [
                [
                    at_date,
                    leaf_sensor_1,
                    temp_sensor,
                    total_hours_1.toFixed(1),
                    start_date,
                    end_date,
                    low_temp,
                    high_temp,
                    wetness_threshold.toString(),
                    hours_list_1.length
                ]
            ]
        });
    }

    if (wetness_2_enabled) {
        returned_datasets.push({
            "name": "sensor_2_days",
            "column_names": [
                "date",
                "wetness_hours"
            ],
            "row_data": hours_list_2
        });
        returned_datasets.push({
            "name": "sensor_2_summary",
            "column_names": [
                "at_date",
                "leaf_sensor",
                "temp_sensor",
                "total_hours",
                "start_date",
                "end_date",
                "lo_temp",
                "hi_temp",
                "threshold",
                "total_days"
            ],
            "row_data": [
                [
                    at_date,
                    leaf_sensor_2,
                    temp_sensor,
                    total_hours_2.toFixed(2),
                    start_date,
                    end_date,
                    low_temp,
                    high_temp,
                    wetness_threshold.toString(),
                    hours_list_1.length
                ]
            ]
        });
    }

    return returned_datasets;
}