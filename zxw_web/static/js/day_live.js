/** JavaScript for live data refresh and records refresh.
 * User: David Goodwin
 * Date: 21/05/12
 * Time: 6:31 PM
 *
 * Back when this script was originally written it was only polling a JSON
 * file every 48 seconds to update the current conditions table. When complexity
 * first appeared with the websocket-based current conditions updating it really
 * ought to have been restructured. Now that records, chart and image
 * auto-updating has been bodged in its really just a mess.
 */

if (!window.console) console = {log: function() {}};

var video_support = !!document.createElement('video').canPlayType;

var socket = null;
var ws_connected = false;
var ws_state = 'conn';
var ws_lost_connection = false;

var live_enabled = true;
var live_started = false;

var e_live_status = $('#live_status');
var e_live_status_cont = $('#live_status_cont');

var poll_interval = null;
var update_check_interval = null;

var davis_forecast_rules = {};

var image_sections = null;

var current_day_of_month = null;

var records = null;

var loading_buttons_disabled = false;

// Set in connect_live() to prevent an infinite loop of server reloading when
// data on the server is old
var server_reload = false;

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

var current_168h_point = null;
var incomplete_168h_points = [];

function list_avg(items) {
    var sum = 0;
    for (var i = 0; i < items.length; i++) {
        sum += items[i];
    }

    return sum/items.length;
}

/*
 * AveragePoint is used for building points in the 168h line graphs.
 */
function AveragePoint(time_stamp) {
    this.time_stamp = time_stamp;
    // Next point is 30 minutes after this one:
    this.next_point_timestamp = new Date(time_stamp.getTime() + (30 * 60 * 1000));
    this._temperature = [];
    this._dew_point = [];
    this._apparent_temperature = [];
    this._wind_chill = [];
    this._humidity = [];
    this._pressure = [];
    this._wind_speed = [];
    this._gust_wind_speed = [];
    this._uv_index = [];
    this._solar_radiation = [];
    this.is_empty = true;
}

AveragePoint.prototype.contains_sample = function(sample) {
    var sample_ts = sample.time_stamp;

    return this.contains_timestamp(sample_ts);
};

AveragePoint.prototype.contains_timestamp = function(time_stamp) {
    return time_stamp >= this.time_stamp &&
        time_stamp < this.next_point_timestamp;
};

AveragePoint.prototype.add_sample = function(sample) {
    if (!this.contains_sample(sample)) {
        // We should never get here.
        return;
    }

    if (this.is_full()) {
        // This data should already be in the point.
        return;
    }

    this._temperature.push(sample.temperature);
    this._dew_point.push(sample.dew_point);
    this._apparent_temperature.push(sample.apparent_temperature);
    this._wind_chill.push(sample.wind_chill);
    this._humidity.push(sample.humidity);
    this._pressure.push(sample.pressure);
    this._wind_speed.push(sample.wind_speed);
    this._gust_wind_speed.push(sample.gust_wind_speed);

    // Conditionally putting the samples in the buffers here is ok as the point
    // timestamps are pre-determined so it won't really affect anything but the
    // value if a few samples go missing for whatever reason.
    if (sample.uv_index != null) {
        this._uv_index.push(sample.uv_index);
    }

    if (sample.solar_radiation != null) {
        this._solar_radiation.push(sample.solar_radiation);
    }

    this.is_empty = false;
};

AveragePoint.prototype.is_full = function() {
    var max_samples = 1800 / sample_interval;

    if (this.is_empty) {
        return false;
    }

    return this._temperature.length >= max_samples;
};

AveragePoint.prototype.temperature = function() {
    if (this.is_empty) return null;

    return list_avg(this._temperature);
};

AveragePoint.prototype.dew_point = function() {
    if (this.is_empty) return null;

    return list_avg(this._dew_point);
};

AveragePoint.prototype.apparent_temperature = function() {
    if (this.is_empty) return null;

    return list_avg(this._apparent_temperature);
};

AveragePoint.prototype.wind_chill = function() {
    if (this.is_empty) return null;

    return list_avg(this._wind_chill);
};

AveragePoint.prototype.humidity = function() {
    if (this.is_empty) return null;

    return list_avg(this._humidity);
};

AveragePoint.prototype.pressure = function() {
    if (this.is_empty) return null;

    return list_avg(this._pressure);
};

AveragePoint.prototype.wind_speed = function() {
    if (this.is_empty) return null;

    return list_avg(this._wind_speed);
};

AveragePoint.prototype.gust_wind_speed = function() {
    if (this.is_empty) return null;

    return list_avg(this._gust_wind_speed);
};

AveragePoint.prototype.uv_index = function() {
    if (this.is_empty || this._solar_radiation.length == 0) return null;

    return list_avg(this._uv_index);
};

AveragePoint.prototype.solar_radiation = function() {
    if (this.is_empty || this._solar_radiation.length == 0) return null;

    return list_avg(this._solar_radiation);
};


if (!String.prototype.trim) {
    (function() {
        // Make sure we trim BOM and NBSP
        var rtrim = /^[\s\uFEFF\xA0]+|[\s\uFEFF\xA0]+$/g;
        String.prototype.trim = function() {
            return this.replace(rtrim, '');
        };
    })();
}

// This converts a date to a form that is parseable by Date.parse (for some
// reason Date doesn't seem to have any built-in methods that are guaranteed
// to do this across browsers. Another opportunity for moment.js
function dateToString(dt) {

    // format: 2017-02-19T07:47:48+13:00

    var hours = dt.getHours();
    var hoursString = hours.toString();
    if (hours < 10) {
        hoursString = "0" + hoursString;
    }

    return dt.getFullYear().toString() + "-" +
            (dt.getMonth() + 1).toString() + "-" +  // 0-based month strikes again
            dt.getDate().toString() + "T" +
            hoursString + ":" +
            dt.getMinutes().toString() + ":" +
            dt.getSeconds().toString() + "+xx:xx";
}

function stringToDate(str) {
    var ts_parts = str.split("T");
    var date = ts_parts[0];
    var time = ts_parts[1];

    var date_parts = date.split("-");
    var year = parseInt(date_parts[0]);
    var month = parseInt(date_parts[1] - 1);  // JS months are 0-based
    var day = parseInt(date_parts[2]);

    // Toss away the timezone. The WebUI assumes all parts of the system are running in the
    // same timezone :(
    var time_parts = time.split("+")[0].split(":");
    var hour = parseInt(time_parts[0]);
    var minute = parseInt(time_parts[1]);
    var second = parseInt(time_parts[2]);

    return new Date(year, month, day, hour, minute, second);
}

// Code for managing an images section client-side
function ImageSection(el, src_code) {
    if (el != null) {
        this.element = el;
    }
    this.src_code = src_code
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
    p.html(description);
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

ImageSection.prototype._element_time_stamp = function(element) {
    var a = $(element).attr("data-time-stamp");
    if (typeof a === typeof undefined || a === false) {
        return null; // No image loaded?
    }

    return stringToDate(a);
};

ImageSection.prototype._element_sort_order = function(element) {
    var a = $(element).attr("data-sort-order");
    if (typeof a === typeof undefined || a === false) {
        return null; // No image loaded?
    }
    return parseInt(a);
};

ImageSection.prototype._element_title = function(element) {
    return $(element).attr("data-title");
};

ImageSection.prototype._compare_element = function(
    element, time_stamp, sort_order, title, ascending) {

    var other_time_stamp = this._element_time_stamp(element);

    if (other_time_stamp === null) {
        return 1; // No image loaded
    }

    var other_sort_order = this._element_sort_order(element);
    var other_title = this._element_title(element);

    // console.log("-----------------");
    // console.log("TS existing [" + other_time_stamp + "], new [" + time_stamp + "]");
    // console.log("SO existing [" + other_sort_order + "], new [" + sort_order + "]");
    // console.log("T existing [" + other_title + "], new [" + title + "]");

    if (time_stamp > other_time_stamp) {
        // console.log("New element is greater than (" + other_time_stamp + ", " +
        //     sort_order + "," + title + ") by timestamp");
        return 1;
    } else if (time_stamp < other_time_stamp) {
        // console.log("New element is less than (" + other_time_stamp + ", " +
        //     sort_order + "," + title + ") by timestamp");
        return -1;
    } else {
        // Note: Ascending: Bigger number is lower priority! 5 vs 10? 5 wins.
        // Descending: Bigger number is higher priority.
        // Promoted image uses ascending, image list uses descending.
        if ((ascending && sort_order < other_sort_order) ||
            (!ascending && sort_order > other_sort_order)) {
            // console.log("New element is greater than (" + other_time_stamp + ", " +
            //     sort_order + "," + title + ") by sort order");
            return 1;
        } else if (sort_order === other_sort_order) {
            // console.log("New element is equal tp (" + other_time_stamp + ", " +
            //     sort_order + "," + title + ") by timestamp and sort order, " +
            //     "falling back to title");
            return title > other_title;
        } else {
            // console.log("New element is less than (" + time_stamp + ", " +
            //     sort_order + "," + title + ") by sort order");
            return -1;
        }
    }
};

ImageSection.prototype._create_image = function(thumb_url, full_url, title,
                                                caption, time_stamp, sort_order) {
    var li = $("<li/>");
    li.attr("data-time-stamp", dateToString(time_stamp));
    li.attr("data-sort-order", sort_order);
    li.attr("data-title", title);

    var caption_width = thumbnail_width - 20; // todo: remove magic number

    var a = $("<a class='thumbnail'/>");
    a.attr('href', full_url);
    var img = $("<img/>");
    img.attr('src', thumb_url);
    img.attr('title', title);
    var div = $("<div class='caption'/>");
    div.attr('style', 'width: ' + caption_width.toString() + 'px;');
    div.text(caption);
    a.append(img);
    a.append(div);
    li.append(a);
    return li;
};

ImageSection.prototype._create_video = function(full_url, title, caption,
                                                time_stamp, sort_order) {
    var li = $("<li/>");
    li.attr("time_stamp", dateToString(time_stamp));
    li.attr("sort_order", sort_order);
    li.attr("title", title);

    var div = $("<div class='thumbnail'/>");
    var video = $("<video controls></video>");
    video.attr('src', full_url);
    video.attr('title', title);
    video.attr('width', thumbnail_width);
    video.attr('preload', 'none');

    var caption_width = thumbnail_width - 20; // todo: remove magic number

    var caption_div = $("<div class='caption'></div>");
    caption_div.attr('style', 'width: ' + caption_width.toString() + 'px;');
    caption_div.text(caption);

    div.append(video);
    div.append(caption_div);

    li.append(div);

    return li;
};

ImageSection.prototype._create_audio = function(full_url, title, caption,
                                                time_stamp, sort_order) {
    var li = $("<li/>");
    li.attr("time_stamp", dateToString(time_stamp));
    li.attr("sort_order", sort_order);
    li.attr("title", title);

    var div = $("<div class='thumbnail'/>");
    var audio = $("<audio controls></audio>");
    audio.attr('src', full_url);
    audio.attr('title', title);
    audio.attr('preload', 'none');

    var caption_width = thumbnail_width - 20; // todo: remove magic number

    var caption_div = $("<div class='caption'></div>");
    caption_div.attr('style', 'width: ' + caption_width.toString() + 'px;');
    caption_div.text(caption);

    div.append(audio);
    div.append(caption_div);

    li.append(div);

    return li;
};

ImageSection.prototype._add_image_to_list = function(
    thumb_url, full_url, title, caption, is_video, is_audio, time_stamp,
    sort_order) {
    var ul = this.element.find("ul");

    var li= null;

    if (is_video) {
        li = this._create_video(full_url, title, caption, time_stamp, sort_order);
    } else if (is_audio) {
        li = this._create_audio(full_url, title, caption, time_stamp, sort_order);
    } else {
        li = this._create_image(thumb_url, full_url, title, caption, time_stamp, sort_order);
    }

    // Now we've got the new list item we've got to slot it into the list in the
    // right location.
    var sect = this;
    var inserted = false;
    $("li", ul).each(function() {
        if (inserted) {
            return;
        }

        var cmp = sect._compare_element(this, time_stamp, sort_order, title, false);

        if (cmp < 0) {
            // console.log("Inserting before:");
            // console.log(this);

            // belongs before the current one.
            li.insertBefore(this);
            inserted = true;
        }
    });

    // Belongs at the end of the list
    if (!inserted) {
        // console.log("Appending to list");
        ul.append(li);
    }

    //adjust_thumbnail_heights();
};

ImageSection.prototype._make_carousel_image = function(full_url, title, caption,
        time_stamp, sort_order, active) {
    var div = $("<div />");
    div.addClass("item");
    if (active) {
        div.addClass("active");
    }
    div.attr("data-title", title);
    div.attr("data-sort-order", sort_order);
    div.attr("data-time-stamp", dateToString(time_stamp));

    var img = $("<img />");
    img.attr("src", full_url);
    img.attr("alt", title);
    img.attr("title", title);
    div.append(img);

    var captionEl = $("<div />");
    captionEl.addClass("carousel-caption");

    var p = $("<p />");
    p.text(caption);
    captionEl.append(p);
    div.append(captionEl);
    return div;
};

ImageSection.prototype._switch_to_carousel = function(current_image_row) {
    // We are switching to a carousel with two images

    // Pull out the details for the currently promoted image. This will be the
    // first image in the carousel
    var full_url = current_image_row.find("img").attr("src");
    var title = this._element_title(current_image_row);
    var caption = current_image_row.find("div.caption").text();
    var time_stamp = this._element_time_stamp(current_image_row);
    var sort_order = this._element_sort_order(current_image_row);

    var elId = this.src_code + "_promoted";

    // Main div
    var div = $("<div />");
    div.attr("id", elId);
    div.addClass("carousel");
    div.addClass("slide");

    // List of images
    var imageList = $("<div />");
    imageList.addClass("carousel-inner");

    if (time_stamp !== null) {
        // If we have a current image, add it to the carousel.
        imageList.append(this._make_carousel_image(full_url, title, caption,
            time_stamp, sort_order, true));
    }
    div.append(imageList);

    // Nav buttons
    var left = $("<a />");
    left.addClass("carousel-control");
    left.addClass("left");
    left.attr("href", "#" + elId);
    left.attr("data-slide", "prev");
    left.html("&lsaquo;");
    div.append(left);

    var right = $("<a />");
    right.addClass("carousel-control");
    right.addClass("right");
    right.attr("href", "#" + elId);
    right.attr("data-slide", "next");
    right.html("&rsaquo;");
    div.append(right);

    // Hide the existing elements
    current_image_row.find("img").hide();
    current_image_row.find("video").hide();
    current_image_row.find("div.caption").hide();

    // Update sorting attributes on the image row
    current_image_row.attr("data-time-stamp", dateToString(time_stamp));
    current_image_row.attr("data-sort-order", sort_order);
    current_image_row.attr("data-title", title);

    // And slot in the carousel
    current_image_row.append(div);

    div.carousel();
};

ImageSection.prototype._add_image_to_carousel = function(current_image_row,
                                                         full_url, title,
                                                         caption, time_stamp,
                                                         sort_order) {

    var item = this._make_carousel_image(full_url, title, caption, time_stamp,
        sort_order, false);

    var carousel_inner = current_image_row.find("div.carousel-inner");

    // Now we've got the new list item we've got to slot it into the list in the
    // right location.
    var sect = this;
    var inserted = false;
    carousel_inner.find(".item").each(function() {
        if (inserted) {
            return;
        }

        var cmp = sect._compare_element(this, time_stamp, sort_order, title, false);

        if (cmp < 0) {
            // belongs before the current one.
            item.insertBefore(this);
            inserted = true;
        }
    });

    // Belongs at the end of the list
    if (!inserted) {
        // console.log("Appending to list");
        carousel_inner.append(item);
    }

    $('.carousel-inner').each(function() {
        var maxHeight = 0;

        $(".item", this).each(function() {
            var h = $(this).height();
            if (h > maxHeight) {
                maxHeight = h;
            }
        });

        // Remove any images that are less than half the height of the
        // largest image (broadcasts during sunrise may produce a few
        // very short images alongside several normal size ones which
        // looks odd)
        try {
            $(".item", this).each(function() {
                var h = $(this).find("img").get(0).naturalHeight;
                if (h < (maxHeight / 2)) {
                    $(this).remove();
                }
            });
        } catch (e) {}

        $(this).find(".item", this).height(maxHeight);
    });
};

ImageSection.prototype._update_current_image = function(full_url, title,
                                                        caption, is_video,
                                                        time_stamp, sort_order) {
    var current_image_row = this.element.find("div.row.current_image");

    var current_ts = this._element_time_stamp(current_image_row);
    var current_sort = this._element_sort_order(current_image_row);

    var carousel_mode = current_image_row.find(".carousel").length > 0;

    if (current_ts !== null && current_ts.getTime() === time_stamp.getTime()
        && current_sort !== sort_order && !is_video) {
        // Another image with the same timestamp but a different sort order!

        if (!carousel_mode) {
            // Deploy Carousel Mode!
            this._switch_to_carousel(current_image_row);
        }

        this._add_image_to_carousel(current_image_row, full_url, title,
                                    caption, time_stamp, sort_order);

        return;
    }

    var cmp = this._compare_element(current_image_row, time_stamp, sort_order,
        title, true /* ascending sort */);

    if (cmp <= 0) {
        return;  // Image has a lower sort index than the current one. Ignore it.
    }

    var img = current_image_row.find("img");
    var video = current_image_row.find("video");
    var caption_element = current_image_row.find("div.caption");

    // Get rid of the carousel - were showing a single image or video now.
    if (carousel_mode) {
        current_image_row.find("div.carousel").remove();
        if (video.length !== 0) {
            console.log("found video el");
            video.show();
        } else if (img.length !== 0) {
            console.log("found img el");
            img.show();
        }
        caption_element.show();
    }

    if (is_video) {
        if (video.length === 0) {
            // We're currently displaying an image. Swap out the img element
            // with a video one.
            video = $('<video width="100%" controls></video>');
            img.replaceWith(video);
        }

        video.attr('src', full_url);
        video.attr('title', title);
    } else {
        if (img.length === 0) {
            // We're currently displaying a video. Swap out the video element
            // with an image one.
            img = $('<img />');
            video.replaceWith(img);
        }

        img.attr('src', full_url);
        img.attr('title', title);
    }

    caption_element.text(caption);

    current_image_row.attr("data-time-stamp", dateToString(time_stamp));
    current_image_row.attr("data-sort-order", sort_order);
    current_image_row.attr("data-title", title);
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

ImageSection.prototype.addImage = function(thumb_url, full_url, title, caption,
                                           is_video, is_audio, time_stamp,
                                           sort_order, type_code) {
    this._add_image_to_list(thumb_url, full_url, title, caption, is_video,
        is_audio, time_stamp, sort_order);

    type_code = type_code.toLowerCase();

    if (type_code === "aptd" || type_code === "spec" || type_code === "apt" || is_audio) {
        // Not worthy of being a promoted image.
        return;
    }

    this._update_current_image(full_url, title, caption, is_video, time_stamp,
         sort_order);
};

function adjust_thumbnail_heights() {
    $('.thumbnails').each(function() {
        var maxHeight = 0;

        $("li", this).each(function() {
            var h = $(this).height();
            if (h > maxHeight) {
                maxHeight = h;
            }
        });

        $("li", this).height(maxHeight);
    })
}


function discover_image_sections() {
    image_sections = {};
    $("section.images").each(function(i,s) {
        var el = $(s);
        var src_code = el.attr('data-src').toLowerCase();
        image_sections[src_code] = new ImageSection(el, src_code);
    });
}

function add_image(parsed) {
    if (image_sections == null) {
        discover_image_sections();
    }

    var station_code = parsed['station_code'];
    var src_code = parsed['source_code'];

    $.getJSON(parsed['description_url'], function(description_data) {
        var title = description_data.title;
        var description = description_data.description;

        if (description === null || description === '') {
            if (title === null) {
                description = parsed['time'];
            } else {
                description = title;
            }
        }

        if (title === null || title === '') {
            title = parsed['time_stamp'].replace('T', ' ');
        }

        if (src_code in image_sections) {
            var section = image_sections[src_code];

            section.addImage(parsed['thumb_url'], parsed['full_url'], title,
                description, parsed['is_video'], parsed['is_audio'],
                parsed['date'], parsed['sort_order'], parsed['type_code']);
        } else {

            // In order to create a new section we have to go and look up its title and description.
            $.getJSON(data_root + station_code + "/image_sources.json", function(data) {
                // Image source codes are in UPPERCASE. We use lowercase here in
                // JS land, so...
                var upperSrcCode = src_code.toUpperCase();

                if (upperSrcCode in data) {
                    var img_src_title = data[upperSrcCode]['name'];
                    var img_src_description = data[upperSrcCode]['description'];

                    var section = new ImageSection(null, src_code);
                    section.create_element(src_code, img_src_title, img_src_description);

                    section.addImage(parsed['thumb_url'], parsed['full_url'],
                        title, description, parsed['is_video'],
                        parsed['is_audio'], parsed['date'],
                        parsed['sort_order'], parsed['type_code']);

                    image_sections[src_code] = section;
                }

            });
        }
    });


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

function drop_images_section() {
    image_sections = null;
    $("section.images").remove();
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

        drop_images_section();

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
    if (parts[0] !== 'l' || parts.length < 13) {
        // Not a live record or not a valid live
        // record (needs at least 12 fields)
        return null;
    }

    console.log(parts[1]);

    var now = new Date(parts[1]);
    var h = now.getHours();
    var m = now.getMinutes();
    var s = now.getSeconds();
    if (h < 10) h = '0' + h;
    if (m < 10) m = '0' + m;
    if (s < 10) s = '0' + s;
    var time = h + ':' + m + ':' + s;

    var wind = parseFloat(parts[11]);
    if (wind_speed_kmh) {
        wind = wind * 3.6
    }

    var result = {
        'temperature': parseFloat(parts[2]),
        'dew_point': parseFloat(parts[3]),
        'apparent_temperature': parseFloat(parts[4]),
        'wind_chill': parseFloat(parts[5]),
        'relative_humidity': parseInt(parts[6]),
        // Indoor temperature - 7
        // Indoor humidity - 8
        'absolute_pressure': parseFloat(parts[9]),
        'msl_pressure': parseFloat(parts[10]),
        'average_wind_speed': wind,
        'wind_direction': parseInt(parts[12]),
        'hw_type': hw_type,
        'time_stamp': time,
        'age': 0,
        's': 'ok',
        'davis': null
    };

    if (hw_type === 'DAVIS') {

        if (parts.length < 29) {
            // Invalid davis record (needs 29 fields)
            return null;
        }

        result['davis'] = {
            'bar_trend': parseInt(parts[13]),
            'rain_rate': parseFloat(parts[14]),
            'storm_rain': parseFloat(parts[15]),
            'current_storm_date': parts[16],
            'tx_batt': parseInt(parts[17]),
            'console_batt': parseFloat(parts[18]),
            'forecast_icon': parseInt(parts[19]),
            'forecast_rule': parseInt(parts[20]),
            'uv_index': parseFloat(parts[21]),
            'solar_radiation': parseInt(parts[22]),
            // 23 - average wind speed 2m
            // 24 - average wind speed 10m
            // 25 - gust wind speed 10m
            // 26 - gust wind speed direction 10m
            // 27 - heat index
            'thsw_index': parseFloat(parts[28])
            // 29 - altimeter setting
            // 30 - leaf wetness 1
            // 31 - leaf wetness 2
            // 32 - leaf temperature 1
            // 33 - leaf temperature 2
            // 34 - soil moisture 1
            // 35 - soil moisture 2
            // 36 - soil moisture 3
            // 37 - soil moisture 4
            // 38 - soil temperature 1
            // 39 - soil temperature 2
            // 40 - soil temperature 3
            // 41 - soil temperature 4
            // 42 - extra temperature 1
            // 43 - extra temperature 2
            // 44 - extra temperature 3
            // 45 - extra humidity 1
            // 46 - extra humidity 2
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
    var mime_type = parts[5].trim().toLowerCase();

    var extension = "jpeg";
    var is_video = false;
    var is_audio = false;

    if (mime_type == "image/jpeg") {
        extension = "jpeg";
    } else if (mime_type == "image/png") {
        extension = "png";
    } else if (mime_type == "image/gif") {
        extension = "gif";
    } else if (mime_type == "video/mp4") {
        extension = "mp4";
        is_video = true;
    }else if (mime_type == "video/webm") {
        extension = "webm";
        is_video = true;
    } else if (mime_type == "audio/wav") {
        extension = "wav";
        is_audio = true;
    } else if (mime_type == "audio/mp3") {
        extension = "mp3";
        is_audio = true
    } else if (mime_type == "audio/flac") {
        extension = "flac";
        is_audio = true
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
    var description_url = url_base + "description.json";

    var sort_order = image_type_sort.split(",").indexOf(type_code.toUpperCase());

    if (sort_order == -1) {
        sort_order = 1000;
    }

    var dt = new Date(year,
        month - 1, // Because january is 0 in javascript for some stupid reason
        day,
        hour,
        minute,
        second);

    return {
        'station_code': station_code,
        'source_code': source_code,
        'type_code': type_code,
        'time_stamp': parts[4],
        'time': h_s + ":" + m_s,
        'full_url': full_url,
        'thumb_url': thumbnail_url,
        'description_url': description_url,
        'is_video': is_video,
        'is_audio': is_audio,
        'sort_order': sort_order,
        'date': dt
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
    var time_string = parts[1].replace("T", " ").split("+")[0];
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
    time_stamp.setMonth(month-1);
    time_stamp.setDate(day);
    time_stamp.setHours(hour);
    time_stamp.setMinutes(minute);
    time_stamp.setSeconds(second);

    var wind = parseNoneableFloat(parts[10]);
    var wind_gust = parseNoneableFloat(parts[11]);
    if (wind_speed_kmh) {
        if (wind != null) {
            wind = wind * 3.6;
        }
        if (wind_gust != null) {
            wind_gust = wind_gust * 3.6;
        }
    }

    var absPressure = parseNoneableFloat(parts[9]);
    var mslPressure = parseNoneableFloat(parts[10]);

    var pressure = mslPressure != null ? mslPressure : absPressure;

    // Note - samples are also constructed in build_average_point_for_ts()
    var sample = {
        // record type  // 0
        time_stamp: time_stamp,
        full_time_stamp: parts[1],
        temperature: parseNoneableFloat(parts[2]),
        dew_point: parseNoneableFloat(parts[3]),
        apparent_temperature: parseNoneableFloat(parts[4]),
        wind_chill: parseNoneableFloat(parts[5]),
        humidity: parseNoneableInt(parts[6]),
        indoor_temperature: parseNoneableFloat(parts[7]),
        indoor_humidity: parseNoneableInt(parts[8]),
        pressure: pressure;
        wind_speed: wind,
        gust_wind_speed: wind_gust,
        wind_direction: parseNoneableInt(parts[13]),
        rainfall: parseNoneableFloat(parts[14]),
        uv_index: null,
        solar_radiation: null
    };

    if (hw_type === 'DAVIS' && parts.length > 15) {
        sample.uv_index = parseNoneableFloat(parts[15]);
        sample.solar_radiation = parseNoneableFloat(parts[16]);
    }

    return sample;
}

function check_for_new_records(sample) {

    var time_stamp = sample.time_stamp;

    var new_records = [];

    if (records == null || sample.temperature > records.max_temp) {
        records.max_temp = sample.temperature;
        records.max_temp_ts = time_stamp;
        new_records.push(['day', 'max_temp', sample.temperature, time_stamp]);
    }
    if (records == null || sample.temperature < records.min_temp) {
        records.min_temp = sample.temperature;
        records.min_temp_ts = time_stamp;
        new_records.push(['day', 'min_temp', sample.temperature, time_stamp]);
    }

    if (records == null || sample.wind_chill > records.max_wind_chill) {
        records.max_wind_chill = sample.wind_chill;
        records.max_wind_chill_ts = time_stamp;
        new_records.push(['day', 'max_wind_chill', sample.wind_chill, time_stamp]);
    }
    if (records == null || sample.wind_chill < records.min_wind_chill) {
        records.min_wind_chill = sample.wind_chill;
        records.min_wind_chill_ts = time_stamp;
        new_records.push(['day', 'min_wind_chill', sample.wind_chill, time_stamp]);
    }

    if (records == null || sample.apparent_temperature > records.max_apparent_temp) {
        records.max_apparent_temp = sample.apparent_temperature;
        records.max_apparent_temp_ts = time_stamp;
        new_records.push(['day', 'max_apparent_temp', sample.apparent_temperature, time_stamp]);
    }
    if (records == null || sample.apparent_temperature < records.min_apparent_temp) {
        records.min_apparent_temp = sample.apparent_temperature;
        records.min_apparent_temp_ts = time_stamp;
        new_records.push(['day', 'min_apparent_temp', sample.apparent_temperature, time_stamp]);
    }

    if (records == null || sample.dew_point > records.max_dew_point) {
        records.max_dew_point = sample.dew_point;
        records.max_dew_point_ts = time_stamp;
        new_records.push(['day', 'max_dew_point', sample.dew_point, time_stamp]);
    }
    if (records == null || sample.dew_point < records.min_dew_point) {
        records.min_dew_point = sample.dew_point;
        records.min_dew_point_ts = time_stamp;
        new_records.push(['day', 'min_dew_point', sample.dew_point, time_stamp]);
    }

    if (records == null || sample.pressure > records.max_pressure) {
        records.max_pressure = sample.pressure;
        records.max_pressure_ts = time_stamp;
        new_records.push(['day', 'max_pressure', sample.pressure, time_stamp]);
    }
    if (records == null || sample.pressure < records.min_pressure) {
        records.min_pressure = sample.pressure;
        records.min_pressure_ts = time_stamp;
        new_records.push(['day', 'min_pressure', sample.pressure, time_stamp]);
    }

    if (records == null || sample.humidity > records.max_humidity) {
        records.max_humidity = sample.humidity;
        records.max_humidity_ts = time_stamp;
        new_records.push(['day', 'max_humidity', sample.humidity, time_stamp]);
    }
    if (records == null || sample.humidity < records.min_humidity) {
        records.min_humidity = sample.humidity;
        records.min_humidity_ts = time_stamp;
        new_records.push(['day', 'min_humidity', sample.humidity, time_stamp]);
    }

    if (records == null || sample.gust_wind_speed > records.max_gust_wind) {
        records.max_gust_wind = sample.gust_wind_speed;
        records.max_gust_wind_ts = time_stamp;
        new_records.push(['day', 'max_gust_wind', sample.gust_wind_speed, time_stamp]);
    }

    if (records == null || sample.wind_speed > records.max_wind) {
        records.max_wind = sample.wind_speed;
        records.max_wind_ts = time_stamp;
        new_records.push(['day', 'max_wind', sample.wind_speed, time_stamp]);
    }

    if (new_records.length > 0) {
        // records changed, update the UI
        update_records_table();
    }
}

function graph_data_sort(a,b) {
    var date_a = a[0];
    var date_b = b[0];
    return date_b>date_a ? -1 : date_b<date_a ? 1 : 0;
}

function add_sample_to_day_graphs(sample) {
    if (data_sets.day == null) {
        return; // Graphs haven't been initialised yet.
    }

    var ts = sample.time_stamp;
    var last_ts = data_sets.day.tdp_data[data_sets.day.tdp_data.length-1][0];
    var earliest_ts = data_sets.day.tdp_data[0][0];

    // If the timestamp on the sample we've just received is earlier than the
    // most recent sample in the graphs, we'll need to sort the graph data to
    // ensure the new sample ends up in the right place.
    var sort_required = ts < last_ts;

    if (ts.getTime() <= earliest_ts.getTime()) {
        console.log('Sample too old for graph');
        return; // Sample doesn't belong in this graph (its more than 24h old)
    }
    // todo: Check the graph doesn't already contain this sample?
    // Its pretty unlikely we'd get a duplicate notification though

    // For 1-day graphs we just add it on to the end.
    // For 24 hour graphs we add it on to the end and remove the first item.

    // TDP is Timestamp, Temperature and Dew Point
    data_sets.day.tdp_data.push([ts, sample.temperature, sample.dew_point]);

    // AWC is Timestamp, Apparent Temperature and Wind Chill
    data_sets.day.awc_data.push([ts, sample.apparent_temperature, sample.wind_chill]);

    // Humidity is Timestamp and Humidity
    data_sets.day.humidity_data.push([ts, sample.humidity]);

    // Pressure is Timestamp and Pressure
    data_sets.day.pressure_data.push([ts, sample.pressure]);

    // Wind Speed is Timestamp, Average wind Speed and Gust Wind Speed
    data_sets.day.wind_speed_data.push([ts, sample.wind_speed, sample.gust_wind_speed]);

    if (data_sets.day.uv_index_data != null) {
        // UV Index is Timestamp and UV Index
        data_sets.day.uv_index_data.push([ts, sample.uv_index]);
    }

    if (data_sets.day.solar_radiation_data != null) {
        // UV Index is Timestamp and Solar Radiation
        data_sets.day.solar_radiation_data.push([ts, sample.solar_radiation]);
    }

    if (sort_required) {
        console.log("sort required.");
        data_sets.day.tdp_data.sort(graph_data_sort);
        data_sets.day.awc_data.sort(graph_data_sort);
        data_sets.day.humidity_data.sort(graph_data_sort);
        data_sets.day.pressure_data.sort(graph_data_sort);
        data_sets.day.wind_speed_data.sort(graph_data_sort);

        if (data_sets.day.uv_index_data != null) {
            data_sets.day.uv_index_data.sort(graph_data_sort);
        }

        if (data_sets.day.solar_radiation_data != null) {
            data_sets.day.solar_radiation_data.sort(graph_data_sort);
        }
    }

    if (!is_day_page) {
        // For the 24 hour graphs we need to remove the oldest item if the graph is full.

        // 86400 seconds in 24 hours. Sample interval is in seconds.
        var sample_count = 86400 / sample_interval;

        if (data_sets.day.tdp_data.length > sample_count) {
            data_sets.day.tdp_data.shift();
            data_sets.day.awc_data.shift();
            data_sets.day.humidity_data.shift();
            data_sets.day.pressure_data.shift();
            data_sets.day.wind_speed_data.shift();

            if (data_sets.day.uv_index_data != null) {
                data_sets.day.uv_index_data.shift();
            }

            if (data_sets.day.solar_radiation_data != null) {
                data_sets.day.solar_radiation_data.shift();
            }
        }
    }

    // Now update the graphs to display the new data.
    data_sets.day.update_graphs();
}

function add_point_to_168h_graph(point) {
    var ts = point.time_stamp;

    console.log('Adding new point to 168h graph with timestamp ' + ts);

    // add new point with above timestamp

    // TDP is Timestamp, Temperature and Dew Point
    data_sets.week.tdp_data.push([ts, point.temperature(), point.dew_point()]);

    // AWC is Timestamp, Apparent Temperature and Wind Chill
    data_sets.week.awc_data.push([ts, point.apparent_temperature(),
        point.wind_chill()]);

    // Humidity is Timestamp and Humidity
    data_sets.week.humidity_data.push([ts, point.humidity()]);

    // Pressure is Timestamp and Pressure
    data_sets.week.pressure_data.push([ts, point.pressure()]);

    // Wind Speed is Timestamp, Average wind Speed and Gust Wind Speed
    data_sets.week.wind_speed_data.push([ts, point.wind_speed(),
        point.gust_wind_speed()]);

    if (data_sets.week.uv_index_data != null) {
        // UV Index is Timestamp and UV Index
        data_sets.week.uv_index_data.push([ts, point.uv_index()]);
    }

    if (data_sets.week.solar_radiation_data != null) {
        // UV Index is Timestamp and Solar Radiation
        data_sets.week.solar_radiation_data.push([ts, point.solar_radiation()]);
    }

    if (data_sets.week.tdp_data.length > 336) {
        // The graph is full. Remove the oldest item
        data_sets.week.tdp_data.shift();
        data_sets.week.awc_data.shift();
        data_sets.week.humidity_data.shift();
        data_sets.week.pressure_data.shift();
        data_sets.week.wind_speed_data.shift();

        if (data_sets.week.uv_index_data != null) {
            data_sets.week.uv_index_data.shift();
        }

        if (data_sets.week.solar_radiation_data != null) {
            data_sets.week.solar_radiation_data.shift();
        }
    }

    data_sets.week.update_graphs();
}

function get_point_index_in_168h_graph(point) {
    var id = data_sets.week.tdp_data.length - 1;

    for (var i = id; i >= 0; i--) {
        var point_ts = data_sets.week.tdp_data[i][0];
        if (point_ts.getTime() == point.time_stamp.getTime()) {
            return i;
        }
    }

    return null;
}

function update_point_in_168h_graph(point) {
    var id = get_point_index_in_168h_graph(point);

    // TDP is Timestamp, Temperature and Dew Point
    data_sets.week.tdp_data[id][1] = point.temperature();
    data_sets.week.tdp_data[id][2] = point.dew_point();

    // AWC is Timestamp, Apparent Temperature and Wind Chill
    data_sets.week.awc_data[id][1] = point.apparent_temperature();
    data_sets.week.awc_data[id][1] = point.wind_chill();

    // Humidity is Timestamp and Humidity
    data_sets.week.humidity_data[id][1] = point.humidity();

    // Pressure is Timestamp and Pressure
    data_sets.week.pressure_data[id][1] = point.pressure();

    // Wind Speed is Timestamp, Average wind Speed and Gust Wind Speed
    data_sets.week.wind_speed_data[id][1] = point.wind_speed();
    data_sets.week.wind_speed_data[id][2] = point.gust_wind_speed();

    if (data_sets.week.uv_index_data != null) {
        // UV Index is Timestamp and UV Index
        data_sets.week.uv_index_data[id][1] = point.uv_index();
    }

    if (data_sets.week.solar_radiation_data != null) {
        // Solar Radiation is Timestamp and Solar Radiation
        data_sets.week.solar_radiation_data[id][1] = point.solar_radiation();
    }
    data_sets.week.update_graphs();
}

function build_average_point_for_ts(time_stamp) {
    var point = new AveragePoint(time_stamp);

    console.log('Building average point from 24h data for: ' + time_stamp);

    // Grab any values in the 24h chart that belong in this 30-minute averaged
    // point.
    for(var i = 0; i < data_sets.day.tdp_data.length - 1; i++) {
        var sample_ts = data_sets.day.tdp_data[i][0];
        if (point.contains_timestamp(sample_ts)) {

            var sample = {
                time_stamp: sample_ts,
                full_time_stamp: null,
                temperature: data_sets.day.tdp_data[i][1],
                dew_point: data_sets.day.tdp_data[i][2],
                apparent_temperature: data_sets.day.awc_data[i][1],
                wind_chill: data_sets.day.awc_data[i][2],
                humidity: data_sets.day.humidity_data[i][1],
                indoor_temperature: null,
                indoor_humidity: null,
                pressure: data_sets.day.pressure_data[i][1],
                wind_speed: data_sets.day.wind_speed_data[i][1],
                gust_wind_speed: data_sets.day.wind_speed_data[i][2],
                wind_direction: null,
                rainfall: null,
                uv_index: null,
                solar_radiation: null
            };

            if (data_sets.day.uv_index_data != null) {
                sample.uv_index = data_sets.day.uv_index_data[i][1];
            }

            if (data_sets.day.solar_radiation_data != null) {
                sample.solar_radiation = data_sets.day.solar_radiation_data[i][1];
            }

            point.add_sample(sample);
        }
    }

    return point;
}

function prune_incomplete_168h_points() {
    // This discards any incomplete points that either no longer exist in the
    // graph or are no longer incomplete.

    var min_ts = data_sets.week.tdp_data[0][0];

    var i = incomplete_168h_points.length;
    while (i--) {
        if (incomplete_168h_points[i].is_full()) {
            // Its full - no more samples to add to it so throw it away
            console.log('Discarding incomplete point ' + incomplete_168h_points[i].time_stamp + ': full');
            incomplete_168h_points.splice(i, 1);
        } else if (incomplete_168h_points[i].time_stamp < min_ts) {
            // Its too old.
            console.log('Discarding incomplete point ' + incomplete_168h_points[i].time_stamp + ': too old');
            incomplete_168h_points.splice(i, 1);
        }
    }
}

function get_point_ts_for_sample_ts(ts) {
    // Note: this function will never (and must never) be called with a
    // timestamp earlier than the earliest timestamp in the graph.

    var start_ts = data_sets.week.tdp_data[0][0];

    if (ts < start_ts) {
        // Too old
        return null;
    }

    // We'll only look forward two weeks ((168h * 2 = 336 points) * 2) to find
    // a period to slot this timestamp into.
    for (var i = 0; i <= 168 * 4; i++) {
        var next_ts = new Date(start_ts.getTime() + (30 * 60 * 1000));

        if (ts >= start_ts && ts < next_ts) {
            return start_ts;
        }

        start_ts = next_ts; // Move forward 30 minutes
    }

    // If the timestamp doesn't fall within two weeks of the oldest point
    // currently in the graph, we'll just start the graphs 30-minute intervals
    // over again with this sample
    return ts;
}

function add_sample_to_168hr_graphs(sample) {

    if (sample.time_stamp < data_sets.week.tdp_data[0][0]) {
        // Sample is too old to be displayed in this graph.
        console.log('Sample too old for graph - ignoring');
        return
    }

    if (current_168h_point == null) {
        // First time updating the 168h graph. Initialise the current point with
        // data from the 24h graph.
        console.log('Building initial current point from 24h data...');
        var latest_point_ts = data_sets.week.tdp_data[data_sets.week.tdp_data.length-1][0];
        current_168h_point = build_average_point_for_ts(latest_point_ts);
    }

    var sample_point_ts = get_point_ts_for_sample_ts(sample.time_stamp);

    if (current_168h_point.contains_sample(sample)) {
        current_168h_point.add_sample(sample);
        update_point_in_168h_graph(current_168h_point);
    } else if (sample.time_stamp >= current_168h_point.next_point_timestamp) {
        // Sample is for a future point.

        if (!current_168h_point.is_full()) {
            // Not finished with current point yet. Chuck it in the list of
            // incomplete points in case more samples for it come through later.
            incomplete_168h_points.push(current_168h_point);
            prune_incomplete_168h_points();
        }

        var previous_point = current_168h_point;
        current_168h_point = new AveragePoint(sample_point_ts);
        current_168h_point.add_sample(sample);

        if (current_168h_point.time_stamp > previous_point.next_point_timestamp) {
            // We've skipped a few points forward. We need to create these
            // (currently empty) points and chuck them in the incomplete points
            // list in case we receive data for them later. Until data for them
            // comes through they'll at least appear as a gap in the graph.

            console.log('New point is a long way in the future! Creating filler...');

            var new_ts = previous_point.next_point_timestamp;
            while (new_ts < current_168h_point.time_stamp) {
                console.log('Blank point at: ' + new_ts);
                var empty_point = new AveragePoint(new_ts);
                add_point_to_168h_graph(empty_point);
                incomplete_168h_points.push(empty_point);
                new_ts = empty_point.next_point_timestamp;
            }
        }

        add_point_to_168h_graph(current_168h_point);
    } else {
        // Sample is not for the current point and its not for a future point.
        // See if it belongs in any of the incomplete points we've got lying
        // around.

        console.log('Sample is for a past point.');

        // Now see if it belongs in a point we're still interested in seeing
        // more data for.
        for (var i = 0; i < incomplete_168h_points.length; i++) {
            if (incomplete_168h_points[i].contains_sample(sample)) {
                console.log('Found point in incomplete points list!');

                if (!incomplete_168h_points[i].is_full()) {
                    // We've found a point the sample belongs in!
                    incomplete_168h_points[i].add_sample(sample);
                    update_point_in_168h_graph(incomplete_168h_points[i]);
                }

                prune_incomplete_168h_points();

                return; // done!
            }
        }

        console.log("Point isn't in incomplete points list...");

        // Not in the above list. The sample could be for a point that already
        // existed in the data set the page started out with...

        // If the point is 23 hours or less we should still have all the other
        // data for its 30 minute block sitting in the 24h chart. So we could
        // still update the existing point in the 168h chart

        var d = new Date();
        d.setHours(d.getHours() - 23);

        if (sample_point_ts > d) {
            var point = build_average_point_for_ts(sample_point_ts);
            if (point.is_full()) {
                // The point is already full! Ignore this sample - its of no use.
                console.log("Constructed point is full. Ignoring extra sample.");
                return;
            }
            point.add_sample(sample);
            update_point_in_168h_graph(point);

            if (!point.is_full()) {
                // Still missing some samples from this point. Put it aside
                // in case more come through.
                console.log("constructed point isn't full yet - putting on incomplete list");
                incomplete_168h_points.push(point);
            }
        } else {
            console.log('No data for point is available. Sample ignored.');
        }

        prune_incomplete_168h_points();
    }
}

function add_rain_to_hourly_rainfall_graph(timestamp, rainfall, graph, hours) {
    if (graph == null) {
        return; // Graph not initialised yet.
    }

    var truncated_ts = new Date(timestamp.getTime());

    truncated_ts.setMinutes(0);
    truncated_ts.setSeconds(0);
    truncated_ts.setMilliseconds(0);

    var found = false;

    // We search in reverse as the point we're looking to update will probably
    // be at the end if its in the graph at all.
    for(var i = graph.data.length - 1; i >= 0; i--) {
        var point_ts = graph.data[i][0];
        if (truncated_ts.getTime() == point_ts.getTime()) {
            graph.data[i][1] = graph.data[i][1] + rainfall;
            found = true;
            break;
        }
    }

    if (!found) {
        // Couldn't find the point in the graph. Add a new one.

        var latest_timestamp = graph.data[graph.data.length - 1][0];

        var next_ts = new Date(latest_timestamp.getTime() + (60 * 60 * 1000));
        while (next_ts < truncated_ts) {
            graph.data.push([next_ts, 0]);
            next_ts = new Date(next_ts.getTime() + (60 * 60 * 1000));
        }

        // Couldn't find the time in the graph. Either we've just started a new
        // hour or the graph is missing a point. Either way, this data needs to be
        // added to the graph
        graph.data.push([truncated_ts, rainfall]);

        if (truncated_ts < latest_timestamp) {
            // The point we just added was older than the most recent point in the
            // graph. We need to sort the data.
            graph.data.sort(graph_data_sort);
        }

        // On day pages we're only adding new points until the end of the day.
        if (!is_day_page) {
            // Each point represents one hour. Throw away points until we only have
            // as many as should be in the graph.
            while (graph.data.length > hours+1) {
                graph.data.shift();
            }
        }
    }

    graph.update_graph();
}

function add_rain_to_24h_rainfall_graph(timestamp, rainfall, graph) {

    if (graph == null) {
        return; // Graph not initialised yet.
    }

    var latest_ts = graph.raw_data[graph.raw_data.length - 1][0];

    graph.raw_data.push([timestamp, rainfall]);

    if (timestamp < latest_ts) {
        // Data point we just added was older than the most recent. Sort the
        // data set to make throwing away the oldest points easy.
        graph.raw_data.sort(graph_data_sort);
    }

    latest_ts = graph.raw_data[graph.raw_data.length - 1][0];

    // Earliest timestamp we'll keep is 24 hours ago.
    var earliest_ts = new Date(latest_ts.getTime() - (24 * 60 * 60 * 1000));

    // Compute total rainfall for the last 24 hours throwing away any points
    // that are too old.
    var total = 0;
    var i = graph.raw_data.length;
    while (i--) {
        var ts = graph.raw_data[i][0];

        if (ts < earliest_ts) {
            // Point is too old - discard!
            graph.raw_data.splice(i, 1);
        } else {
            // Point is ok! Add it to the total
            total += graph.raw_data[i][1];
        }
    }

    // Update total rainfall value
    $("#tot_rainfall").text(total.toFixed(1));

    add_rain_to_hourly_rainfall_graph(timestamp, rainfall, graph, 24);
}

function add_sample_to_rainfall_graphs(sample) {
    var ts = sample.time_stamp;
    var rain = sample.rainfall;

    add_rain_to_24h_rainfall_graph(ts, rain, data_sets.rainfall_day);
    add_rain_to_hourly_rainfall_graph(ts, rain, data_sets.rainfall_week, 168);
}

function add_sample_to_graphs(sample) {

    if (ui != 's') {
        // Only the standard UI supports auto-refreshing graphs.
        return;
    }

    add_sample_to_day_graphs(sample);
    add_sample_to_168hr_graphs(sample);
    add_sample_to_rainfall_graphs(sample);
}

function disable_refresh_buttons() {
    if (!loading_buttons_disabled) {
        var buttons = null;

        if (ui == 's') {
            buttons = $(".refreshbutton");
        } else {
            // The alternate UI doesn't support auto-refreshing graphs so we'll
            // only disable the records refresh button.
            buttons = $("#btn_records_refresh");
        }
        buttons.hide();
        loading_buttons_disabled = true;
    }
}

function enable_refresh_buttons() {
    if (loading_buttons_disabled) {
        if (ui == 's') {
            buttons = $(".refreshbutton");
        } else {
            // The alternate UI doesn't support auto-refreshing graphs so only
            // the records refresh button is ever hidden
            buttons = $("#btn_records_refresh");
        }
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
            check_for_new_records(parsed);
            add_sample_to_graphs(parsed);
        }
    } else if (parts[0] === "i") {
        // Image
        parsed = parse_image_data(parts);

        if (parsed != null) {

            var today = new Date();
            today.setHours(0);
            today.setMinutes(0);
            today.setSeconds(0);
            today.setMilliseconds(0);

            if (parsed['date'] < today) {
                console.log("Image too old for this page. Ignoring");
                return; // Image is for a previous day. Ignore it
            }

            if (parsed['is_video'] || parsed['is_audio']) {
                if (video_support) {
                    add_image(parsed);
                }
                // Else browser doesn't support audio/video. Ignore the image
            } else {
                add_image(parsed);
            }
        }
    }
}

function poll_live_data() {
    $.getJSON(live_url, function (data) {
        if (wind_speed_kmh) {
            var wind = data['average_wind_speed'];
            if (wind != null) {
                wind = wind * 3.6;
            }
            data['average_wind_speed'] = wind;
        }
        refresh_live_data(data);
    }).error(function() {
        show_live_data_error("Live data refresh failed - unable to contact the web server");
    });
}

function ws_data_arrived(evt) {
    if (evt.data == '_ok\n' && ws_state == 'conn') {
        var catchup = '';

        if (data_sets != null && data_sets.day != null) {
            // Fetch any new samples that have arrived between the graphs being loaded (potentially
            // from cache) and when the subscribe command is run.
            var last_record = data_sets.day.tdp_data[data_sets.day.tdp_data.length - 1];

            if (last_record[0].toISOString) {
                var ts = last_record[0].toISOString();
                catchup = '/from_timestamp="' + ts + '"';
            }
        }

        socket.send('subscribe "' + station_code + '"/live=2/samples=2/any_order/images' + catchup + '\n');

        ws_state = 'sub';
        console.log('Subscription started.');

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
    disable_refresh_buttons();
    ws_state = 'conn';
    ws_connected = true;
    ws_lost_connection = false;
    socket.send('set client "zxw_web"/version="1.0.0"\n');
    socket.send('set environment "browser_UserAgent" "' +
        navigator.userAgent.replace('"', '\\"') + '"\n');
}

function update_live_status(icon, message) {
    e_live_status_cont.attr('data-original-title', message);
    e_live_status.attr('class', 'fm_status_' + icon);
    e_live_status.tooltip();

    if (icon === 'red') {
        $("#live_status_time").hide();
    } else {
        $("#live_status_time").show();
    }
}

function ws_connect(evt) {
    update_live_status('green', 'Connected');
    finish_connection();
}

function wss_connect(evt) {
    update_live_status('green', 'Connected (wss fallback)');
    finish_connection();
}

function ws_error(evt) {
    console.log('Websocket connection error.');
    console.log('Switching to polling and scheduling reconnect attempt for 1 minute.');

    // Something went wrong trying to connect the websocket. Switch
    // to polling and try again in a minute.
    update_live_status('yellow',
    'Updating automatically every 30 seconds.');
    start_polling();
    setTimeout(function(){attempt_reconnect()},60000);
}

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
    if (!live_started) {
        return; // We're not supposed to be here.
    }

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

    if (!live_started) {
        return; // We're not supposed to be here.
    }

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

function show_live_data_error(msg) {
    console.log("Live data error: " + msg);
    $("#cc_error_msg").html(msg);
    $("#cc_error").show();
    $("#current_conditions").hide();
    update_live_status('red', 'Error');
}

function show_stale_live_data_error(age) {
    show_live_data_error(
            "No live data available. Live data has not been refreshed in "
            + age.toString() + " seconds. The update service may " +
            "have stopped");
}

function hide_live_data_error(show_cc) {
    $("#cc_error").hide();
    if (show_cc) {
        $("#current_conditions").show();
    }
}

function connect_live() {
    if (window.MozWebSocket) {
        window.WebSocket = window.MozWebSocket;
    }

    if(window.WebSocket) {
        if (window.data_sets && data_sets != null && data_sets.day != null) {
            // If the latest point in the graphs is >2h ago we can't catchup
            // from the websocket - we've got to rebuild the graphs using the
            // JSON data sources.
            var last_record = data_sets.day.tdp_data[data_sets.day.tdp_data.length - 1][0];
            var max_age = new Date(new Date().getTime() - (110*60*1000)); // 1h50m ago

            // TODO: the charts on the server may themselves be too old.
            // This can result in an infinite loop of chart reloading because
            // the reloaded ones are still too old. This needs to be smarter or
            // gone.
            if (last_record < max_age && !server_reload) {
                console.log("Chart data too old - reload datasets from server...");

                // Note that we're reloading charts from the server. Setting
                // this prevents an infinite loop of server reloading when the
                // latest data on the server is older than max age
                server_reload = true;

                // Live data hasn't started yet. Turn this off so we can retry
                // once the charts have finished loading.
                live_started = false;

                // This will start live data again once all charts have loaded.
                samples_loading = false;
                rainfall_loading = false;
                samples_7_loading = false;
                rainfall_7_loading = false;
                refresh_day_charts();
                refresh_7day_charts();

                return;
            } else if (server_reload) {
                // Data on the server is too old to load the charts.
                server_reload = false;
                show_live_data_error(
                    "Unable to start live update of current conditions - " +
                    "the data on the server is stale. The update service may " +
                    "have stopped.");
                update_live_status('red', 'Unable to connect - server data stale');
                return;
            }
        }

        // Browser seems to support websockets. Try that if we can.
        if (wss_uri != null || ws_uri != null) {
            try {
                if (wss_uri != null) {
                    attempt_wss_connect();
                } else if (ws_uri != null) {
                    attempt_ws_connect();
                }
            } catch (ex) {
                console.log('Connect failed:' + ex);
                ws_error()
            }
        }else {
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
    var current_conditions = $("#current_conditions");

    if (data['s'] == "bad") {
        show_live_data_error("No live data available. This is likely due to misconfiguration");
    }
    // If the live data is over 5 minutes old show a warning instead.
    else if (data['age'] > 300) {
        show_stale_live_data_error(data['age']);
    } else {
        hide_live_data_error(true);

        // Current conditions
        var relative_humidity = data['relative_humidity'];
        var temperature = data['temperature'].toFixed(1);
        var apparent_temp = data['apparent_temperature'].toFixed(1);
        var wind_chill = data['wind_chill'].toFixed(1);
        var dew_point = data['dew_point'].toFixed(1);
        var pressure = null;
        if (data['msl_pressure'] != null) {
            pressure = data['msl_pressure'].toFixed(1);
        } else {
            pressure = data['absolute_pressure'].toFixed(1);
        }
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
        $("#live_pressure").html(pressure + ' hPa' + bar_trend);

        if (wind_speed_kmh) {
            $("#live_avg_wind_speed").html(wind_speed + ' km/h (' + bft_data[1] + ')');
        } else {
            $("#live_avg_wind_speed").html(wind_speed + ' m/s (' + bft_data[1] + ')');
        }
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
        show_stale_live_data_error('over 300');
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

    var wind_unit = " m/s at ";
    if (wind_speed_kmh) {
        wind_unit = " km/h at ";
    }
    $("#gws").html(records.max_gust_wind.toFixed(1) + wind_unit + formatTime(records.max_gust_wind_ts));
    $("#aws").html(records.max_wind.toFixed(1) + wind_unit + formatTime(records.max_wind_ts));
}

// This function refreshes the records object and updates the display.
function reload_records() {
    $("#btn_records_refresh").button('loading');
    $.getJSON(records_url, function(data) {

        var wind = data['max_average_wind_speed'];
        var wind_gust = data['max_gust_wind_speed'];
        if (wind_speed_kmh) {
            if (wind != null) {
                wind = wind * 3.6;
            }
            if (wind_gust != null) {
                wind_gust = wind_gust * 3.6;
            }
        }

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

            min_pressure: data['min_pressure'],
            min_pressure_ts: parseTime(data['min_pressure_ts']),
            max_pressure: data['max_pressure'],
            max_pressure_ts: parseTime(data['max_pressure_ts']),

            min_humidity: data['min_humidity'],
            min_humidity_ts: parseTime(data['min_humidity_ts']),
            max_humidity: data['max_humidity'],
            max_humidity_ts: parseTime(data['max_humidity_ts']),

            max_gust_wind: wind_gust,
            max_gust_wind_ts: parseTime(data['max_gust_wind_speed_ts']),
            max_wind: wind,
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

function start_day_live() {
    if (s_mode_live_start_interval != null) {
        window.clearInterval(s_mode_live_start_interval);
    }

    if (live_auto_refresh && !live_started) {
        console.log('Starting data subscription...');

        live_started = true;

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
}

var s_mode_live_start_interval = null;

if (ui != 's') {
    // On the standard UI we auto-update the graphs so we've got to wait for
    // them to finish loading before starting the live subscription.
    start_day_live();
} else if (ui == 's') {
    // Sometimes the charts don't start live data properly (possibly due to
    // browser caching). This will wait 30 seconds after page load and start
    // live data if it hasn't already been started.
    s_mode_live_start_interval = window.setInterval(function() {
        console.log('Live data not started after 30 seconds. Starting.');
        start_day_live();
    }, 30000);
}