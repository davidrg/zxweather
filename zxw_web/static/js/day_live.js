/** JavaScript for live data refresh and records refresh.
 * User: David Goodwin
 * Date: 21/05/12
 * Time: 6:31 PM
 */

var socket = null;
var ws_connected = false;
var ws_state = 'conn';
var ws_lost_connection = false;

var live_enabled = true;

var e_live_status = $('#live_status');
var e_live_status_cont = $('#live_status_cont');

var poll_interval = null;
var update_check_interval = null;

var davis_forecast_rules = {};

var image_sections = null;

var current_day_of_month = null;

var records = null;

var loading_buttons_disabled = false;

var months = {
  1 : 'january',
  2 : 'february',
  3 : 'march',
  4 : 'april',
  5 : 'may',
  6 : 'june',
  7 : 'july',
  8 : 'august',
  9 : 'september',
  10: 'october',
  11: 'november',
  12: 'december'
};


// Code for managing an images section client-side
function ImageSection(el) {
    if (el != null) {
        this.element = el;
    }
}

ImageSection.prototype._create_title_row = function(title) {
    var title_row = $("<div class='row'>");
    var span = $("<span class='span6'/>");
    var h2 = $("<h2/>");
    h2.text(title);
    span.append(h2);
    title_row.append(span);

    return title_row;
};

ImageSection.prototype._create_description_row = function(description) {
    if (description == null) {
        return null;
    }

    var row = $("<div class='row'/>");
    var span = $("<div class='span12'/>");
    var p = $("<p/>");
    p.text(description);
    span.append(p);
    row.append(span);

    return row;
};

ImageSection.prototype._create_latest_image_row = function() {
    var row = $("<div class='row current_image'/>");
    var span = $("<div class='span12'/>");
    var img = $("<img/>");
    var div = $("<div class='caption'/>");
    span.append(img);
    span.append(div);
    row.append(span);

    return row;
};

ImageSection.prototype._create_images_row = function() {
    var row = $("<div class='row'/>");
    var span = $("<div class='span12'/>");
    span.append($("<h3>All images</h3>"));
    span.append($("<ul class='thumbnails'/>"));
    row.append(span);
    return row;
};

ImageSection.prototype._create_image = function(thumb_url, full_url, title, caption) {
    var li = $("<li/>");
    var a = $("<a class='thumbnail'/>");
    a.attr('href', full_url);
    var img = $("<img/>");
    img.attr('src', thumb_url);
    img.attr('title', title);
    var div = $("<div class='caption'/>");
    div.text(caption);
    a.append(img);
    a.append(div);
    li.append(a);
    return li;
};

ImageSection.prototype._add_image_to_list = function(thumb_url, full_url, title, caption) {
    var image = this._create_image(thumb_url, full_url, title, caption);
    var ul = this.element.find("ul");
    ul.append(image);
};

ImageSection.prototype._update_current_image = function(full_url, title, caption) {
    var current_image_row = this.element.find("div.row.current_image");
    var img = current_image_row.find("img");
    var caption_element = current_image_row.find("div.caption");

    img.attr('src', full_url);
    img.attr('title', title);

    caption_element.text(caption);
};

ImageSection.prototype.create_element = function(code, title, description) {
    var el = $("<section class='images'/>");
    el.attr('data-src', code);

    // Create section rows
    var title_row = this._create_title_row(title);
    var description_row = this._create_description_row(description);
    var latest_image_row = this._create_latest_image_row();
    var images_row = this._create_images_row();

    el.append(title_row);
    el.append(description_row);
    el.append(latest_image_row);
    el.append(images_row);

    $("#history").before(el);

    this.element = el;
};

ImageSection.prototype.addImage = function(thumb_url, full_url, title, caption) {
    this._add_image_to_list(thumb_url, full_url, title, caption);
    this._update_current_image(full_url, title, caption);
};

function discover_image_sections() {
    image_sections = {};
    $("section.images").each(function(i,s) {
        var el = $(s);
        var src_code = el.attr('data-src').toLowerCase();
        image_sections[src_code] = new ImageSection(el);
    });
}

function add_image(parsed) {
    if (image_sections == null) {
        discover_image_sections();
    }

    var station_code = parsed['station_code'];
    var src_code = parsed['source_code'];

    if (src_code in image_sections) {
        var section = image_sections[src_code];
        section.addImage(parsed['thumb_url'], parsed['full_url'], parsed['time_stamp'].replace('T', ' '),
            parsed['time']);
    } else {

        // In order to create a new section we have to go and look up its title and description.
        $.getJSON(data_root + station_code + "/image_sources.json", function(data) {
            if (src_code in data) {
                var title = data[src_code]['name'];
                var description = data[src_code]['description'];

                var section = new ImageSection(null);
                section.create_element(src_code, title, description);

                section.addImage(parsed['thumb_url'], parsed['full_url'],
                    parsed['time_stamp'], parsed['time']);

                image_sections[src_code] = section;
            }

        });
    }
}

function year_url(year) {
    return year.toString() + "/";
}

function month_url(year, month) {
    return year.toString() + "/" + months[month] + "/";
}

function day_url(year, month, day) {
    return year.toString() + "/" + months[month] + "/" + day.toString() + "/";
}

function yesterday_url() {
    var yesterday = new Date();
    yesterday.setDate(yesterday.getDate() - 1);
    return day_url(yesterday.getFullYear(),
                   yesterday.getMonth()+1,
                   yesterday.getDate());
}

// This function handles keeping pages with a live data feed up-to-date when the date changes.
// For day-level pages this means turning off live data and putting it into archive mode.
// For the station page it means resetting the images and records and updating nav links.
function change_page_date() {
    if (is_day_page) {
        // We're on the page for a specific date and the current date has changed. Put the
        // page into archive mode. This means turning off live data, removing the current
        // conditions stuff, removing the refresh buttons and turning on the navigation
        // button to go to the next day.

        // We don't need to reload the graphs or anything as this page previously had a live
        // data feed so everything should already be up-to-date.

        // Prevent WebSockets, etc, from reconnecting
        live_enabled = false;

        // Disable polling
        if (poll_interval != null) {
            window.clearInterval(poll_interval);
            poll_interval = null;
        }

        // Close the data update WebSocket
        if (socket != null) {
            socket.close();
            socket = null;
        }

        // Create a new records section
        var rec_sec = $("<section id='records'/>");

        // Heading
        var row = $("<div class='row'/>");
        var span = $("<div class='span6'/>");
        span.append($("<h2>Records</h2>"));
        row.append(span);
        row.append($("<div class='span6'/>")); // For padding
        rec_sec.append(row);

        // Transplant the records table
        row = $("<div class='row'/>");
        span = $("<div class='span6'/>");
        span.append($("#records_table"));
        row.append(span);
        row.append($("<div class='span6'/>")); // For padding
        rec_sec.append(row);

        // Transplant the total rainfall thing
        row = $("<div class='row'/>");
        span = $("<div class='span6'/>");
        var p = $("<p>Total Rainfall:&nbsp;</p>");
        p.append($("#tot_rainfall"));
        p.append("mm");
        span.append(p);
        row.append(span);
        row.append($("<div class='span6'/>")); // For padding
        rec_sec.append(row);

        var liveEl = $("#live");

        // Add in the new records section
        liveEl.after(rec_sec);

        // Get rid of the old live data section.
        liveEl.remove();

        // Remove all the refresh buttons
        $("button").remove();

        var dayChartsEl = $("#1day");

        // Rename "Today" to "1-day charts"
        dayChartsEl.find("h2").text("1-day Charts");

        // Ditch total rainfall <p>
        dayChartsEl.find("p.total_rainfall").remove();

        // drop latest image and all images heading
        $("div.row.current_image").remove();
        $("div.image_list").find("h3").remove();

        // Enable next day link
        var next_day_button = $("#next_day");
        next_day_button.attr('href', next_day_button.attr('data-url'));
        next_day_button.removeClass('disabled');
    } else {
        // Date has changed on the station overview page. Update navigation URLs and drop all
        // image sections (they'll be recreated as image sources take their first images for the day)

        image_sections = null;
        $("section.images").remove();

        var today = new Date();

        // Update navigation links
        $("#nav_yesterday").find("a").attr('href', yesterday_url());
        $("#nav_this_month").find("a").attr('href', month_url(today.getFullYear(), today.getMonth()+1));
        $("#nav_this_year").find("a").attr('href', year_url(today.getFullYear()));

        // reset records
        records = null;
    }
}

function check_for_date_change() {
    // Check if the date has changed. If it has we may need to make some
    // page structure changes.
    var today = new Date();

    if (current_day_of_month == null) {
        current_day_of_month = today.getDate();
    }

    if (current_day_of_month != today.getDate()) {
        change_page_date();
        current_day_of_month = today.getDate();
        return true;
    }

    return false;
}

function parse_live_data(parts) {
    if (parts[0] != 'l' || parts.length < 11) {
        // Not a live record or not a valid live
        // record (needs at least 12 fields)
        return null;
    }

    var now = new Date();
    var h = now.getHours();
    var m = now.getMinutes();
    var s = now.getSeconds();
    if (h < 10) h = '0' + h;
    if (m < 10) m = '0' + m;
    if (s < 10) s = '0' + s;
    var time = h + ':' + m + ':' + s;

    var result = {
        'temperature': parseFloat(parts[1]),
        'dew_point': parseFloat(parts[2]),
        'apparent_temperature': parseFloat(parts[3]),
        'wind_chill': parseFloat(parts[4]),
        'relative_humidity': parseInt(parts[5]),
        // Indoor temperature - 6
        // Indoor humidity - 7
        'absolute_pressure': parseFloat(parts[8]),
        'average_wind_speed': parseFloat(parts[9]),
        'wind_direction': parseInt(parts[10]),
        'hw_type': hw_type,
        'time_stamp': time,
        'age': 0,
        's': 'ok',
        'davis': null
    };

    if (hw_type == 'DAVIS') {

        if (parts.length != 21) {
            // Invalid davis record (needs 20 fields)
            return null;
        }

        result['davis'] = {
            'bar_trend': parseInt(parts[11]),
            'rain_rate': parseFloat(parts[12]),
            'storm_rain': parseFloat(parts[13]),
            'current_storm_date': parts[14],
            'tx_batt': parseInt(parts[15]),
            'console_batt': parseFloat(parts[16]),
            'forecast_icon': parseInt(parts[17]),
            'forecast_rule': parseInt(parts[18]),
            'uv_index': parseFloat(parts[19]),
            'solar_radiation': parseInt(parts[20])
        };

        if (result['davis']['current_storm_date'] == 'None')
            result['davis']['current_storm_date'] = null;
    }

    return result
}

function parse_image_data(parts) {
    if (parts[0] != "i" || parts.length < 6) {
        // needs to be an image and to have at least 5 parts
        return null;
    }

    // "parse" the timestamp. It is in ISO8601 format:
    // 2016-05-09T17:10:06+12:00
    var ts_parts = parts[4].split("T");
    var date = ts_parts[0];
    var time = ts_parts[1];

    var date_parts = date.split("-");
    var year = parseInt(date_parts[0]);
    var month = parseInt(date_parts[1]);
    var day = parseInt(date_parts[2]);

    // Toss away the timezone. The WebUI assumes all parts of the system are running in the
    // same timezone :(
    var time_parts = time.split("+")[0].split(":");
    var hour = parseInt(time_parts[0]);
    var minute = parseInt(time_parts[1]);
    var second = parseInt(time_parts[2]);

    var station_code = parts[1].toLowerCase();
    var source_code = parts[2].toLowerCase();
    var type_code = parts[3].toLowerCase();
    var mime_type = parts[4].toLowerCase();

    var extension = "jpeg";

    if (mime_type == "image/jpeg") {
        extension = "jpeg";
    } else if (mime_type == "image/png") {
        extension = "png";
    } else if (mime_type == "image/gif") {
        extension = "gif";
    } else if (mime_type == "video/mp4") {
        extension = "mp4";
    }else if (mime_type == "video/webm") {
        extension = "webm";
    }

    var h_s = hour.toString();
    if (hour < 10) {
        h_s = "0" + h_s;
    }

    var m_s = minute.toString();
    if (minute < 10) {
        m_s = "0" + m_s;
    }

    var s_s = second.toString();
    if (second < 10) {
        s_s = "0" + s_s;
    }

    var url_base = data_root + station_code
        + "/" + year.toString() + "/" + month.toString() + "/" + day.toString()
        + "/images/" + source_code + "/" + h_s + "_" + m_s + "_" + s_s
        + "/" + type_code + "_";

    var full_url = url_base + "full." + extension;
    var thumbnail_url = url_base + "thumbnail." + extension;

    return {
        'station_code': station_code,
        'source_code': source_code,
        'type_code': type_code,
        'time_stamp': parts[4],
        'time': h_s + ":" + m_s,
        'full_url': full_url,
        'thumb_url': thumbnail_url
    };
}

function parseNoneableInt(value) {
    if (value == 'None') {
        return null;
    }
    return parseInt(value);
}

function parseNoneableFloat(value) {
    if (value == 'None') {
        return null;
    }
    return parseFloat(value);
}

function parse_sample(parts) {
    if (parts[0] != 's' || parts.length < 14) {
        // Not a sample record or not a valid sample
        // record (needs at least 12 fields)
        return null;
    }

    // TODO: find a library to parse all this (moment.js probably)
    // need to review browser support, etc, first. May have to wait
    // for the big v2.0 refactoring/upgrade.
    var time_string = parts[1];
    var date_string = time_string.split(' ')[0];
    time_string = time_string.split(' ')[1];

    var date_parts = date_string.split('-');
    var year = parseInt(date_parts[0]);
    var month = parseInt(date_parts[1]);
    var day = parseInt(date_parts[2]);

    var time_parts = time_string.split(':');
    var hour = parseInt(time_parts[0]);
    var minute = parseInt(time_parts[1]);
    var second = parseInt(time_parts[2]);

    var time_stamp = new Date();
    time_stamp.setYear(year);
    time_stamp.setMonth(month);
    time_stamp.setDate(day);
    time_stamp.setHours(hour);
    time_stamp.setMinutes(minute);
    time_stamp.setSeconds(second);


    var sample = {
        // record type  // 0
        time_stamp: time_stamp,
        temperature: parseNoneableFloat(parts[2]),
        dew_point: parseNoneableFloat(parts[3]),
        apparent_temperature: parseNoneableFloat(parts[4]),
        wind_chill: parseNoneableFloat(parts[5]),
        humidity: parseNoneableInt(parts[6]),
        indoor_temperature: parseNoneableFloat(parts[7]),
        indoor_humidity: parseNoneableInt(parts[8]),
        pressure: parseNoneableFloat(parts[9]),
        wind_speed: parseNoneableFloat(parts[10]),
        gust_wind_speed: parseNoneableFloat(parts[11]),
        wind_direction: parseNoneableInt(parts[12]),
        rainfall: parseNoneableFloat(parts[13])
    };

    if (hw_type == 'DAVIS' && parts.length > 14) {
        sample.uv_index = parseNoneableFloat(parts[14]);
        sample.solar_radiation = parseNoneableFloat(parts[15]);
    }

    return sample;
}

function check_for_new_records(sample) {

    if (records == null) {
        return;  // No records to compare with
    }

    var new_records = [];

    if (sample.temperature > records.max_temp) {
        records.max_temp = sample.temperature;
        records.max_temp_ts = sample.time_stamp;
        new_records.push(['day', 'max_temp', sample.temperature, sample.time_stamp]);
    }
    if (sample.temperature < records.min_temp) {
        records.min_temp = sample.temperature;
        records.min_temp_ts = sample.time_stamp;
        new_records.push(['day', 'min_temp', sample.temperature, sample.time_stamp]);
    }

    if (sample.wind_chill > records.max_wind_chill) {
        records.max_wind_chill = sample.wind_chill;
        records.max_wind_chill_ts = sample.time_stamp;
        new_records.push(['day', 'max_wind_chill', sample.wind_chill, sample.time_stamp]);
    }
    if (sample.wind_chill < records.min_wind_chill) {
        records.min_wind_chill = sample.wind_chill;
        records.min_wind_chill_ts = sample.time_stamp;
        new_records.push(['day', 'min_wind_chill', sample.wind_chill, sample.time_stamp]);
    }

    if (sample.apparent_temperature > records.max_apparent_temp) {
        records.max_apparent_temp = sample.apparent_temperature;
        records.max_apparent_temp_ts = sample.time_stamp;
        new_records.push(['day', 'max_apparent_temp', sample.apparent_temperature, sample.time_stamp]);
    }
    if (sample.apparent_temperature < records.min_apparent_temp) {
        records.min_apparent_temp = sample.apparent_temperature;
        records.min_apparent_temp_ts = sample.time_stamp;
        new_records.push(['day', 'min_apparent_temp', sample.apparent_temperature, sample.time_stamp]);
    }

    if (sample.dew_point > records.max_dew_point) {
        records.max_dew_point = sample.dew_point;
        records.max_dew_point_ts = sample.time_stamp;
        new_records.push(['day', 'max_dew_point', sample.dew_point, sample.time_stamp]);
    }
    if (sample.dew_point < records.min_dew_point) {
        records.min_dew_point = sample.dew_point;
        records.min_dew_point_ts = sample.time_stamp;
        new_records.push(['day', 'min_dew_point', sample.dew_point, sample.time_stamp]);
    }

    if (sample.pressure > records.max_pressure) {
        records.max_pressure = sample.pressure;
        records.max_pressure_ts = sample.time_stamp;
        new_records.push(['day', 'max_pressure', sample.pressure, sample.time_stamp]);
    }
    if (sample.pressure < records.min_pressure) {
        records.min_pressure = sample.pressure;
        records.min_pressure_ts = sample.time_stamp;
        new_records.push(['day', 'min_pressure', sample.pressure, sample.time_stamp]);
    }

    if (sample.humidity > records.max_humidity) {
        records.max_humidity = sample.humidity;
        records.max_humidity_ts = sample.time_stamp;
        new_records.push(['day', 'max_humidity', sample.humidity, sample.time_stamp]);
    }
    if (sample.humidity < records.min_humidity) {
        records.min_humidity = sample.humidity;
        records.min_humidity_ts = sample.time_stamp;
        new_records.push(['day', 'min_humidity', sample.humidity, sample.time_stamp]);
    }

    if (sample.gust_wind_speed > records.max_gust_wind) {
        records.max_gust_wind = sample.gust_wind_speed;
        records.max_gust_wind_ts = sample.time_stamp;
        new_records.push(['day', 'max_gust_wind', sample.gust_wind_speed, sample.time_stamp]);
    }

    if (sample.wind_speed > records.max_wind) {
        records.max_wind = sample.wind_speed;
        records.max_wind_ts = sample.time_stamp;
        new_records.push(['day', 'max_wind', sample.wind_speed, sample.time_stamp]);
    }

    if (new_records.length > 0) {
        // records changed, update the UI
        update_records_table();
    }
}

function add_sample_to_graphs(sample) {
    // TODO: update all the graphs somehow.

    // For 24 hour graphs we just remove the oldest sample and insert the new one onto the end.


    /*
    The 168 hour graphs present a bit of a challenge as they're a 30 minute average. So the first time
    we have to do anything with them we've got to go on a bit of a discovery process to gather all
    the data that makes up the most recent 30 minute average. From there we'll just keep a buffer
    which we'll average every time a new sample comes in and reset every 30 minutes.

    So the basic procedure for 168 hour graphs is:

    (1) When a new sample comes in and our buffer is null:
      * A 168h sample consists of (30/sample_interval) samples
      * Get the timestamp for the current (latest) 168h sample in the graphs
      * Next 168h sample is due at (last_sample_ts) + 30 minutes
      * IF (last_sample_ts) + 30 minutes == NOW() then SKIP to (2)
      * Current 168h sample is made of ((now - last_sample_ts) / sample_interval) samples.
      * Grab the ((now - last_sample_ts) / sample_interval) most recent samples from the end
        of the 24h graphs and store these in the buffer
      * SKIP to (3)
    (2) When a new sample comes in and we've already got 30 minutes of data in our buffer we'll:
      * Clear the buffer
      * Insert the new sample into the buffer
      * Remove the earliest sample from all 168 hour graphs
      * Recompute 30 minute average
      * Insert new sample at the end of the graph
     (3) When a new sample comes in and we've got less than 30 minutes of data in our buffer we'll:
       * Insert the new sample into the buffer
       * Recompute 30 minute average
       * Update most recent sample in the graphs
     */
}

function disable_refresh_buttons() {
    if (!loading_buttons_disabled) {
        //var buttons = $(".refreshbutton");
        var buttons = $("#btn_records_refresh");
        buttons.hide();
        loading_buttons_disabled = true;
    }
}

function enable_refresh_buttons() {
    if (loading_buttons_disabled) {
        //var buttons = $(".refreshbutton");
        var buttons = $("#btn_records_refresh");
        buttons.show();
        buttons.button('reset');
        loading_buttons_disabled = false;
    }
}

function data_arrived(data) {
    if (check_for_date_change()) {
        // Don't process any further data if its a day page and the day has ended.
        if (is_day_page) {
            return;
        }
    }

    var parts = data.split(',');
    var parsed = null;
    if (parts[0] === "l") {
        // Current conditions
        parsed = parse_live_data(parts);
        if (parsed != null) {
            refresh_live_data(parsed);
        }
    } else if (parts[0] === "s") {
        // Sample
        parsed = parse_sample(parts);
        if (parsed != null) {
            disable_refresh_buttons();
            check_for_new_records(parsed);
            add_sample_to_graphs(parsed);
            // TODO: update rainfall somehow
            // This will probably be a fourth data type:
            // r,hour,hour-rain,24h-rain,day-rain
            // triggered from the database whenever a sample is inserted with non-zero rain data
        }
    } else if (parts[0] === "i") {
        // Image
        parsed = parse_image_data(parts);
        if (parsed != null) {
            add_image(parsed);
        }
    }
}

function poll_live_data() {
    $.getJSON(live_url, function (data) {
        refresh_live_data(data);
    }).error(function() {
            $("#cc_refresh_failed").show();
            $("#current_conditions").hide();
            $("#cc_stale").hide();
        });
}

function ws_data_arrived(evt) {
    if (evt.data == '_ok\r\n' && ws_state == 'conn') {
        socket.send('subscribe "' + station_code + '"/live/samples/images\r\n');
        ws_state = 'sub';

    } else if (ws_state == 'sub') {
        if (evt.data[0] != '#') {
            data_arrived(evt.data);
        }
    }
}

function finish_connection() {
    // Cancel polling
    if (poll_interval != null) {
        window.clearInterval(poll_interval);
        poll_interval = null;
    }
    ws_state = 'conn';
    ws_connected = true;
    ws_lost_connection = false;
    socket.send('set client "zxw_web"/version="1.0.0"\r\n');
    socket.send('set environment "browser_UserAgent" "' +
        navigator.userAgent.replace('"', '\\"') + '"\r\n');
}

function update_live_status(icon, message) {
    e_live_status_cont.attr('data-original-title', message);
    e_live_status.attr('class', 'fm_status_' + icon);
    e_live_status.tooltip();
}

function ws_connect(evt) {
    update_live_status('green', 'Connected');
    finish_connection();
}

function wss_connect(evt) {
    update_live_status('green', 'Connected (wss fallback)');
    finish_connection();
}

function ws_error(evt) { }

function start_polling() {

    enable_refresh_buttons();

    if (poll_interval != null)
        window.clearInterval(poll_interval);

    // Refresh live data every 30 seconds.
    poll_interval = window.setInterval(function(){
        poll_live_data();
    }, 30000);
}

function attempt_ws_connect() {
    socket = new WebSocket(ws_uri);
    socket.onmessage = ws_data_arrived;
    socket.onopen = ws_connect;
    socket.onerror = ws_error;
    socket.onclose = function(evt) {
        if (!live_enabled) {
            // Live data has been disabled. Don't reconnect.
            return;
        }

        if (!ws_connected) {
            if (typeof wss_uri === 'undefined' || wss_uri == null) {
                // wss not enabled.
                update_live_status('yellow',
                    'Connect failed. Polling for updates every 30 seconds.');
                start_polling();
            } else {
                update_live_status('grey',
                    'Connect failed. Attempting wss fallback...');
                attempt_wss_connect();
            }
        } else {
            ws_connected = false;
            ws_lost_connection = true;
            update_live_status('grey',
                'Lost connection. Attempting reconnect...');
            connect_live()
        }
    };
}

function attempt_reconnect() {
    update_live_status('yellow', 'Attempting reconnect...');
    connect_live();
}

function attempt_wss_connect() {
    socket = new WebSocket(wss_uri);
    socket.onmessage = ws_data_arrived;
    socket.onopen = wss_connect;
    socket.onerror = ws_error;
    socket.onclose = function(evt) {
        if (!ws_connected) {
            if (ws_lost_connection) {
                // We've lost the connection and reconnect failed. Re-schedule
                // a connect later.
                setTimeout(function(){attempt_reconnect()},60000);
                update_live_status('yellow',
                    'Reconnect failed. Retrying in one minute.');
            } else {
                update_live_status('yellow',
                    'Connect failed. Polling for updates every 30 seconds.');
                start_polling();
            }

        } else {
            ws_connected = false;
            ws_lost_connection = true;
            update_live_status('grey',
                'Lost connection. Attempting reconnect...');
            connect_live()
        }
    };
}

function connect_live() {
    if (window.MozWebSocket) {
        window.WebSocket = window.MozWebSocket;
    }

    if(window.WebSocket) {
        // Browser seems to support websockets. Try that if we can.
        if (wss_uri != null)
            attempt_wss_connect();
        else if (ws_uri != null)
            attempt_ws_connect();
        else {
            update_live_status('green',
                'Updating automatically every 30 seconds.');
            start_polling();
        }
    } else {
        update_live_status('yellow',
            'Browser too old for instant updates. Polling for updates every ' +
                '30 seconds.');
        start_polling();
    }
}

// wind_speed(m/s) = 0.837B^(2/3) where B is Beaufort scale number
// These numbers are quite approximate.
bft_scale = [
    // Max wind speed, BFT Number, Description, colour
    [0.3,   0, 'Calm', '#FFFFFF'],
    [2,     1, 'Light air', '#CCFFFF'],
    [3,     2, 'Light breeze', '#99FFCC'],
    [5.4,   3, 'Gentle breeze', '#99FF99'],
    [8,     4, 'Moderate breeze', '#99FF66'],
    [10.7,  5, 'Fresh breeze', '#99FF00'],
    [13.8,  6, 'Strong breeze', '#CCFF00'],
    [17.1,  7, 'High wind, moderate gale, near gale', '#FFFF00'],
    [20.6,  8, 'Gale, fresh gale', '#FFCC00'],
    [24.4,  9, 'Strong gale', '#FF9900'],
    [28.3, 10, 'Storm, whole gale', '#FF6600'],
    [32.5, 11, 'Violent storm', '#FF3300'],
    [9999, 12, 'Hurricane', '#FF0000']
    // Speeds over 9999m/s are probably still a hurricane but if you're really
    // getting those wind speeds then BFT accuracy probably isn't your biggest
    // problem.
];


/** Gets Beaufort force for the specified wind speed (in m/s)
 * The return value is a list containing [number, description, colour]
 * @param wind_speed Wind speed in m/s
 */
function bft(wind_speed) {
    for (var i = 0; i < bft_scale.length; i++)
        if (wind_speed < bft_scale[i][0])
            return [bft_scale[i][1], bft_scale[i][2], bft_scale[i][3]];

    return null;
}


var wind_directions = [
    "N", "NNE", "NE", "ENE", "E", "ESE", "SE", "SSE", "S", "SSW", "SW",
    "WSW", "W", "WNW", "NW", "NNW"
];

/** Refreshes the live data display from the supplied dict.
 * @param data Data to populate the display with.
 */
function refresh_live_data(data) {
    $("#cc_refresh_failed").hide();

    var cc_bad = $("#cc_bad");
    var cc_stale = $("#cc_stale");
    var current_conditions = $("#current_conditions");

    cc_bad.hide();
    if (data['s'] == "bad") {
        current_conditions.hide();
        cc_bad.show();
    }
    // If the live data is over 5 minutes old show a warning instead.
    else if (data['age'] > 300) {
        $("#cc_data_age").html(data['age']);
        current_conditions.hide();
        cc_stale.show();
    } else {
        current_conditions.show();
        cc_stale.hide();

        // Current conditions
        var relative_humidity = data['relative_humidity'];
        var temperature = data['temperature'].toFixed(1);
        var apparent_temp = data['apparent_temperature'].toFixed(1);
        var wind_chill = data['wind_chill'].toFixed(1);
        var dew_point = data['dew_point'].toFixed(1);
        var absolute_pressure = data['absolute_pressure'].toFixed(1);
        var wind_speed = data['average_wind_speed'].toFixed(1);
        var hw_type = data['hw_type'];
        var wind_direction = data['wind_direction'];
        var rain_rate = 0;
        var current_storm_rain = null;
        var bft_data = bft(wind_speed);
        var uv_index = 0;
        var solar_radiation = 0;

        var bar_trend = '';

        if (hw_type == 'DAVIS') {
            var bar_trend_val = data['davis']['bar_trend'];
            if (bar_trend_val == -60)
                bar_trend = ' (falling rapidly)';
            else if (bar_trend_val == -20)
                bar_trend = ' (falling slowly)';
            else if (bar_trend_val == 0)
                bar_trend = ' (steady)';
            else if (bar_trend_val == 20)
                bar_trend = ' (rising slowly)';
            else if (bar_trend_val == 60)
                bar_trend = ' (rising rapidly)';

            rain_rate = data['davis']['rain_rate'].toFixed(1);
            uv_index = data['davis']['uv_index'];
            solar_radiation = data['davis']['solar_radiation'];

            var storm_rain = data['davis']['storm_rain'].toFixed(1);
            var storm_date = data['davis']['current_storm_date'];
            if (storm_date != null) {
                current_storm_rain = storm_rain + ' mm from ' + storm_date;
            }

            var forecast_icon = data['davis']['forecast_icon'];
            var e_forecast_icon = $('#forecast_icon');
            if (forecast_icon == 8)
                e_forecast_icon.attr('class', 'fc_clear');
            else if (forecast_icon == 6)
                e_forecast_icon.attr('class', 'fc_partly_cloudy');
            else if (forecast_icon == 2)
                e_forecast_icon.attr('class', 'fc_mostly_cloudy');
            else if (forecast_icon == 3)
                e_forecast_icon.attr('class', 'fc_mostly_cloudy_rain');
            else if (forecast_icon == 18)
                e_forecast_icon.attr('class', 'fc_mostly_cloudy_snow');
            else if (forecast_icon == 19)
                e_forecast_icon.attr('class', 'fc_mostly_cloudy_snow_or_rain');
            else if (forecast_icon == 7)
                e_forecast_icon.attr('class', 'fc_partly_cloudy_rain');
            else if (forecast_icon == 22)
                e_forecast_icon.attr('class', 'fc_partly_cloudy_snow');
            else if (forecast_icon == 23)
                e_forecast_icon.attr('class', 'fc_partly_cloudy_snow_or_rain');

            if (data['davis']['forecast_rule'] in davis_forecast_rules) {
                $('#forecast_text').html(
                    davis_forecast_rules[data['davis']['forecast_rule']]);
            }

            if (!is_day_page) {
                // The battery alert panels are only on the station overview page.
                var con_batt_voltage = data['davis']['console_batt'];
                if (con_batt_voltage < 3.5) {
                    $('#con_batt_alert').show();
                    $('#con_voltage').html(con_batt_voltage);
                }

                var tx_batt_status = data['davis']['tx_batt'];
                if (tx_batt_status > 0) {
                    $('#tx_batt_alert').show();
                }
            }
        }

        if (wind_speed == 0.0)
            wind_direction = '--';
        else {
            var index = Math.floor(((wind_direction * 100 + 1125) % 36000) / 2250);
            wind_direction += '\u00B0 (' + wind_directions[index] + ')'
        }

        $("#live_relative_humidity").html(relative_humidity + '%');
        $("#live_temperature").html(temperature + '°C');
        $("#live_apparent_temperature").html(apparent_temp + '°C');
        $("#live_wind_chill").html(wind_chill + '°C');
        $("#live_dew_point").html(dew_point + '°C');
        $("#live_absolute_pressure").html(absolute_pressure + ' hPa' + bar_trend);
        $("#live_avg_wind_speed").html(wind_speed + ' m/s (' + bft_data[1] + ')');
        $("#live_wind_direction").html(wind_direction);
        $("#current_time").html(data['time_stamp']);

        if (hw_type == 'DAVIS') {
            $('#rain_rate').html(rain_rate + ' mm/h');
            var e_storm_rain = $('#storm_rain');
            if (current_storm_rain != null)
                e_storm_rain.html(current_storm_rain);
            else
                e_storm_rain.html('--');

            if (solar_and_uv_available) {
                $("#uv_index").html(uv_index);
                $("#solar_radiation").html(solar_radiation + " W/m&sup2;");
            }
        }

    }

    // This is for handling missing updates from the web-socket update method.
    if (update_check_interval != null)
        window.clearInterval(update_check_interval);
    update_check_interval = window.setInterval(function(){
        $("#cc_data_age").html('over 300');
        current_conditions.hide();
        cc_stale.show();
    }, 300000);
}

function parseTime(time) {
    var bits = time.split(":");

    var today = new Date();
    today.setHours(parseInt(bits[0]));
    today.setMinutes(parseInt(bits[1]));
    today.setSeconds(parseInt(bits[2]));

    return today;
}

function formatTime(time) {
    // there must be a better way of doing this.
    var t = "";

    if (time.getHours() < 10) {
        t = "0" ;
    }
    t += time.getHours().toString();
    t += ":";

    if (time.getMinutes() < 10) {
        t += "0" ;
    }
    t += time.getMinutes().toString();
    t += ":";

    if (time.getSeconds() < 10) {
        t += "0" ;
    }
    t += time.getSeconds().toString();

    return t;
}

// Updates the records table from the current values in the records object
function update_records_table() {
    $("#temp_min").html(records.min_temp.toFixed(1) + '°C at ' + formatTime(records.min_temp_ts));
    $("#temp_max").html(records.max_temp.toFixed(1) + '°C at ' + formatTime(records.max_temp_ts));

    $("#wc_min").html(records.min_wind_chill.toFixed(1) + '°C at ' + formatTime(records.min_wind_chill_ts));
    $("#wc_max").html(records.max_wind_chill.toFixed(1) + '°C at ' + formatTime(records.max_wind_chill_ts));

    $("#at_min").html(records.min_apparent_temp.toFixed(1) + '°C at ' + formatTime(records.min_apparent_temp_ts));
    $("#at_max").html(records.max_apparent_temp.toFixed(1) + '°C at ' + formatTime(records.max_apparent_temp_ts));

    $("#dp_min").html(records.min_dew_point.toFixed(1) + '°C at ' + formatTime(records.min_dew_point_ts));
    $("#dp_max").html(records.max_dew_point.toFixed(1) + '°C at ' + formatTime(records.max_dew_point_ts));

    $("#ap_min").html(records.min_pressure.toFixed(1) + ' hPa at ' + formatTime(records.min_pressure_ts));
    $("#ap_max").html(records.max_pressure.toFixed(1) + ' hPa at ' + formatTime(records.max_pressure_ts));

    $("#rh_min").html(records.min_humidity + '% at ' + formatTime(records.min_humidity_ts));
    $("#rh_max").html(records.max_humidity + '% at ' + formatTime(records.max_humidity_ts));

    $("#gws").html(records.max_gust_wind.toFixed(1) + ' m/s at ' + formatTime(records.max_gust_wind_ts));
    $("#aws").html(records.max_wind.toFixed(1) + ' m/s at ' + formatTime(records.max_wind_ts));
}

// This function refreshes the records object and updates the display.
function reload_records() {
    $("#btn_records_refresh").button('loading');
    $.getJSON(records_url, function(data) {
        records = {
            min_temp: data['min_temperature'],
            min_temp_ts: parseTime(data['min_temperature_ts']),
            max_temp: data['max_temperature'],
            max_temp_ts: parseTime(data['max_temperature_ts']),

            min_wind_chill: data['min_wind_chill'],
            min_wind_chill_ts: parseTime(data['min_wind_chill_ts']),
            max_wind_chill: data['max_wind_chill'],
            max_wind_chill_ts: parseTime(data['max_wind_chill_ts']),

            min_apparent_temp: data['min_apparent_temperature'],
            min_apparent_temp_ts: parseTime(data['min_apparent_temperature_ts']),
            max_apparent_temp: data['max_apparent_temperature'],
            max_apparent_temp_ts: parseTime(data['max_apparent_temperature_ts']),

            min_dew_point: data['min_dew_point'],
            min_dew_point_ts: parseTime(data['min_dew_point_ts']),
            max_dew_point: data['max_dew_point'],
            max_dew_point_ts: parseTime(data['max_dew_point_ts']),

            min_pressure: data['min_absolute_pressure'],
            min_pressure_ts: parseTime(data['min_absolute_pressure_ts']),
            max_pressure: data['max_absolute_pressure'],
            max_pressure_ts: parseTime(data['max_absolute_pressure_ts']),

            min_humidity: data['min_humidity'],
            min_humidity_ts: parseTime(data['min_humidity_ts']),
            max_humidity: data['max_humidity'],
            max_humidity_ts: parseTime(data['max_humidity_ts']),

            max_gust_wind: data['max_gust_wind_speed'],
            max_gust_wind_ts: parseTime(data['max_gust_wind_speed_ts']),
            max_wind: data['max_average_wind_speed'],
            max_wind_ts: parseTime(data['max_average_wind_speed_ts'])
        };

        $("#records_table").show();
        $("#rec_refresh_failed").hide();

        update_records_table();

        $("#btn_records_refresh").button('reset');
    }).error(function() {
            $("#records_table").hide();
            $("#rec_refresh_failed").show();
            $("#btn_records_refresh").button('reset');
        });
}

if (live_auto_refresh) {
    if (hw_type == 'DAVIS') {
        // Load Davis forecast rules
        $.getJSON(forecast_rules_uri, function (data) {
            davis_forecast_rules = data;
        });
    }

    disable_refresh_buttons();

    // Trigger a refresh so we have a copy of the records in JS land
    // which we can keep up-to-date.
    reload_records();

    connect_live();
}