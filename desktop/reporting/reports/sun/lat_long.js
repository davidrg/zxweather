if (!String.prototype.repeat) {
  String.prototype.repeat = function(count) {
    'use strict';
    if (this == null) {
      throw new TypeError('can\'t convert ' + this + ' to object');
    }
    var str = '' + this;
    count = +count;
    if (count != count) {
      count = 0;
    }
    if (count < 0) {
      throw new RangeError('repeat count must be non-negative');
    }
    if (count == Infinity) {
      throw new RangeError('repeat count must be less than infinity');
    }
    count = Math.floor(count);
    if (str.length == 0 || count == 0) {
      return '';
    }
    // Ensuring count is a 31-bit integer allows us to heavily optimize the
    // main part. But anyway, most current (August 2014) browsers can't handle
    // strings 1 << 28 chars or longer, so:
    if (str.length * count >= 1 << 28) {
      throw new RangeError('repeat count must not overflow maximum string size');
    }
    var maxCount = str.length * count;
    count = Math.floor(Math.log(count) / Math.log(2));
    while (count) {
       str += str;
       count--;
    }
    str += str.substring(0, maxCount - str.length);
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

function generate_lat_long_dataset(criteria) {

    var latitude = criteria["latitude"];
    var lat = dms(latitude);
    if (latitude < 0) {
        lat += " S";
    } else {
        lat += " N";
    }

    var longitude = criteria["longitude"];
    var long = dms(longitude);
    if (longitude < 0) {
        long += " W";
    } else {
        long += " E";
    }

    return {
		"name": "lat_long",
		"column_names": [
            "lat", "long", "lat_dec", "long_dec"
        ],
		"row_data": [
            [
                lat,
                long,
                latitude.toFixed(6),
                longitude.toFixed(6)
            ]
        ]
	};
}
