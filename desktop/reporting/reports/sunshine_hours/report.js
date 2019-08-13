/****************************************************************************
 * Bright Sunshine Hours: data processing script                            *
 ****************************************************************************
 * This script handles calculation of sunshine hours as it is not possible  *
 * to implement the astronomical calculations in SQLite (no trig functions) *
 * and is difficult to implement in PostgreSQL (done it before - lots of    *
 * CTEs)                                                                    *
 *                                                                          *
 * This script also handles generation of a number of additional summary    *
 * data sets which are used for display purposes.                           *
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
    if (count === Infinity) {
      throw new RangeError('repeat count must be less than infinity');
    }
    count |= 0; // floors and rounds-down it.
    if (str.length === 0 || count === 0) {
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

function dms(val) {
    var deg = Math.floor(Math.abs(val));
    var min = Math.floor(60 * (Math.abs(val) - deg));
    var sec = Math.round(3600 * (Math.abs(val) - deg) - 60 * min);

    var result =  deg + "° " + min + "' " + sec + '"';
    return result.padStart(12);
}

// noinspection JSUnusedGlobalSymbols
function transform_datasets(criteria, datasets) {
	
	// Look for the dataset containing query results
	var dataSetIdx;
	for(var i = 0; i < datasets.length; i++) {
		console.log(datasets[i]["name"]);
		if (datasets[i]["name"] === "sun_data") {
			dataSetIdx = i;
		}
	}
    
    if (dataSetIdx === undefined) {
        return []; // Couldn't find the dataset. No data sets modified.
    }
    
    // Find the columns.
    var column_index = {};
    for (i = 0; i < datasets[dataSetIdx]["column_names"].length; i++) {
        var columnName = datasets[dataSetIdx]["column_names"][i];
        column_index[columnName] = i;
    }
    var idxTimeStamp = column_index["time_stamp"];
    var idxSolarRadiation = column_index["solar_radiation"];
    var idxSampleInterval = column_index["sample_interval"];
    var idxMaxSun = column_index["max_solar_radiation"];
    var idxSunHours = column_index["bright_sunshine_hours"];
    var idxImageLink = column_index["image_link"];
    
    var algorithm = SOLAR_ALG_BRAS;
    if (criteria["alg_ryan_stolzenbach"]) {
        algorithm = SOLAR_ALG_RYAN_STOLZENBACH;
    }
    var turbidity = criteria["atmospheric_turbidity"];
    var transmission_factor = criteria["atmospheric_transmission_coefficient"];
    var threshold = criteria["threshold"];
    var minimum_solar_radiation = criteria["minimum_solar"];
    
    var latitude = criteria["latitude"];
    var longitude = criteria["longitude"];
    var altitude = criteria["altitude"];
    
    var daily_summary = [];
    var current_date = "";
    var current_date_ts = moment(criteria["start"]);
    var current_date_image_link = null;
    var current_date_plot_link = null;
    var current_start_ts = null;
    var current_end_ts = null;
    var current_total = 0;
    var current_max = 0;
    var total_hours = 0;
    var max_date = moment(criteria["start"]);
    var max_hours = 0;
    var any_images = false;

    var txt_format_rows = [];

    // // Loop through and see if we've got any images
    // for(i = 0; i < datasets[dataSetIdx]["row_data"].length; i++) {
    //     var x = datasets[dataSetIdx]["row_data"][i][idxImageLink];
    //     if (x !== null && x !== "") {
    //         any_images = true;
    //         break;
    //     }
    // }

    // Loop through rows calculating stuff
    for(i = 0; i < datasets[dataSetIdx]["row_data"].length; i++) {
        var ts = moment(datasets[dataSetIdx]["row_data"][i][idxTimeStamp]);
        var solar_radiation = datasets[dataSetIdx]["row_data"][i][idxSolarRadiation];
        var sample_interval_seconds = datasets[dataSetIdx]["row_data"][i][idxSampleInterval];
        var image_link = datasets[dataSetIdx]["row_data"][i][idxImageLink];
        var sample_interval_hours = sample_interval_seconds / 60.0 / 60.0;
        
        var max_solar_radiation = solar_max(algorithm, ts, latitude, longitude, altitude, transmission_factor, turbidity);
        
        var sunshine_hours = 0;
        if ((solar_radiation > max_solar_radiation * threshold / 100.0) && solar_radiation >= minimum_solar_radiation) {
            sunshine_hours = sample_interval_hours;
        }
        
        var this_date = ts.format("DD/MM/YY");

        if (current_date === "") {
            current_date = this_date;
            current_date_ts = ts;
            current_date_image_link = null;
			current_date_plot_link = null;
            current_start_ts = ts;
            current_end_ts = null;

        }

        if (image_link !== null && image_link !== "" && current_date_image_link == null) {
            current_date_image_link = '<a href="zxw://view-images?date=' + image_link + '">View Images</a>';
        }

        if (current_date_plot_link == null) {
            current_date_plot_link = '<a href="zxw://plot?start=' + current_date_ts.startOf('day').toISOString() + '&end=' + current_date_ts.endOf('day').toISOString() + '&title=Solar Radiation&graphs=solar_radiation">Plot</a>';
        }


        if (current_date !== this_date) {
            var links = current_date_plot_link;

            if (current_date_image_link !== null) {
                links = links + ", " + current_date_image_link;
            }

            var row = [
                current_date_ts.format("DD-MMM-YYYY"),
                current_date_ts.format("YYYY-MM-DD"),
                current_total.toFixed(1),
                current_max.toFixed(1),
                (current_max > 0 ? current_total/current_max * 100 : 0.0).toFixed(1),
                links
            ];

            daily_summary.push(row);
            txt_format_rows.push([
                current_date,
                current_total.toFixed(1).toString().padStart(4, ' ')
            ]);

            current_date = this_date;
            current_date_ts = ts;
            current_date_image_link = null;
			current_date_plot_link = null;
            current_total = 0;
            current_max = 0;
            current_start_ts = ts;
        }
        
        current_total += sunshine_hours;
        total_hours += sunshine_hours;

        if (max_solar_radiation > 0) {
            current_max += sample_interval_hours;
            max_hours += sample_interval_hours;
        }
        
        datasets[dataSetIdx]["row_data"][i][idxMaxSun] = max_solar_radiation;
        datasets[dataSetIdx]["row_data"][i][idxSunHours] = sunshine_hours;

        if (ts > max_date) {
            max_date = ts;
        }
    }

    var end_date = max_date;

    if (current_date !== "") {
        links = current_date_plot_link;
        if (current_date_image_link !== null) {
            links = links + ", " + current_date_image_link;
        }

        row = [
                current_date_ts.format("DD-MMM-YYYY"),
                current_date_ts.format("YYYY-MM-DD"),
                current_total.toFixed(1),
                current_max.toFixed(1),
                (current_max > 0 ? current_total/current_max * 100:0).toFixed(1),
                links
            ];

        daily_summary.push(row);

        txt_format_rows.push([
            current_date,
            current_total.toFixed(1).toString().padStart(4, ' ')
        ]);
    }

    var day_count = end_date.diff(moment(criteria["start"]), 'days') + 1;

    if (txt_format_rows.length < day_count) {
        var blanks = day_count - txt_format_rows.length;
        for (i = 0; i < blanks; i++) {
            txt_format_rows.push([
                "  ----",
                ""
            ]);
        }
    }

    var lat = dms(latitude);
    if (latitude < 0) {
        lat += " S";
    } else {
        lat += " N";
    }

    var long = dms(longitude);
    if (longitude < 0) {
        long += " W";
    } else {
        long += " E";
    }

    // We only have to return modified datasets.
    return [
        datasets[dataSetIdx],
        {
            "name": "daily_summary",
            "column_names": [
                "date",
                "sortable_date",
                "total_sun_hours",
                "max_sun_hours",
                "percentage",
                "links"
            ],
            "row_data": daily_summary
        },{
            "name": "txt_day_summary",
            "column_names": [
                "date",
                "total_sun_hours"
            ],
            "row_data": txt_format_rows
        }, {
            "name": "info",
            "column_names": [
                // stuff for the wlformat template
                "run_date",
                "total_hours",
                "start_date",
                "end_date",
                "threshold",
                "count",
                "total_max_hours",
                "total_percent"
            ],
            "row_data": [
                [
                    // stuff for the wlformat template
                    moment().format("DD/MM/YY"),
                    total_hours.toFixed(1).toString().padStart(6, ' '),
                    moment(criteria["start"]).format("DD/MM/YY"),
                    end_date.format("DD/MM/YY"),
                    criteria["threshold"],
                    txt_format_rows.length,
                    max_hours.toFixed(1),
                    (max_hours > 0 ? total_hours / max_hours * 100 : 0.0).toFixed(1)
                ]
            ]
        }, {
            "name": "lat_long",
            "column_names": [
                "lat", "long"
            ],
            "row_data": [
                [lat, long]
            ]
	    }, {
            "name": "sun_data",
            "column_names": datasets[dataSetIdx]["column_names"],
            // Only return the first 1000 rows. Large amounts of data is
            // slow and can crash the application under some versions of Qt.
            "row_data": datasets[dataSetIdx]["row_data"].slice(0,1000)
        }
    ];
}