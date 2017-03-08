# coding=utf-8
import mimetypes

from database import get_image_sources_for_station, get_images_for_source


class StationImage(object):
    _img_url_fmt = '/data/{station_code}/{year}/{month}/{day}/images/' \
                   '{source_code}/{time}/{type}_{mode}{extension}'

    def __init__(self, img_data, url_prefix=""):
        self._time_stamp = img_data.time_stamp
        self._mime = img_data.mime_type
        self._id = img_data.id
        self._src = img_data.source
        self._station = img_data.station
        self._url_prefix = url_prefix
        self._type = img_data.type_code.lower()
        self._title = img_data.title
        self._description = img_data.description
        self._sort_order = img_data.sort_order

        self._extension = mimetypes.guess_extension(self._mime)
        if self._extension == ".jpe":
            self._extension = ".jpeg"

        self._day = self._time_stamp.date()

    @property
    def is_video(self):
        return self._mime.startswith("video/")

    @property
    def is_audio(self):
        return self._mime.startswith("audio/")

    @property
    def title(self):
        return self._title

    @property
    def description(self):
        if self._description is None:
            return ""
        return self._description

    @property
    def thumbnail_url(self):

        if self.is_video:
            return None

        return self._url_prefix + self._img_url_fmt.format(
            station_code=self._station.lower(),
            year=self._day.year,
            month=self._day.month,
            day=self._day.day,
            source_code=self._src.lower(),
            time=self._time_stamp.strftime("%H_%M_%S"),
            type=self._type,
            mode="thumbnail",
            extension=self._extension
        )

    @property
    def url(self):
        return self._url_prefix + self._img_url_fmt.format(
            station_code=self._station.lower(),
            year=self._day.year,
            month=self._day.month,
            day=self._day.day,
            source_code=self._src.lower(),
            time=self._time_stamp.strftime("%H_%M_%S"),
            type=self._type,
            mode="full",
            extension=self._extension
        )

    @property
    def time(self):
        return self._time_stamp

    @property
    def time_of_day(self):
        return self._time_stamp.time().strftime("%H:%M")

    @property
    def date(self):
        return self._time_stamp.date()

    @property
    def js_date(self):
        return self._time_stamp.isoformat()
        #return self._time_stamp.strftime("%Y/%m/%d %H:%M:%S")

    @property
    def image_id(self):
        return self._id

    @property
    def sort_order(self):
        return self._sort_order


def get_station_day_images(station_id, day):
    # Get images
    images = []
    sources = get_image_sources_for_station(station_id)
    if sources is not None:
        # We have image sources for this station! Lets see if there are any
        # actual images for this date.
        for source in sources:
            image_itr = get_images_for_source(source.image_source_id, day)
            source_images = None
            if image_itr is not None:
                source_images = []
                for image in image_itr:
                    source_images.append(StationImage(image, '../../../../..'))
            if source_images is not None and (not len(source_images) > 0):
                source_images = None

            if source_images is not None and len(source_images) > 0:

                latest_images = [x for x in source_images
                                if x.image_id == source.last_image_id]
                latest_image = None
                if len(latest_images) > 0:
                    latest_image = latest_images[0]

                images.append({
                    'name': source.source_name,
                    'description': source.description,
                    'code': source.code,
                    'images': source_images,
                    'latest': latest_image
                })
    return images


def get_all_station_images(station_id):
    images = []
    sources = get_image_sources_for_station(station_id)
    if sources is not None:
        # The station has one or more sources. Grab any images.
        for source in sources:
            image_itr = get_images_for_source(source.image_source_id)
            source_images = None
            if image_itr is not None:
                source_images = []
                for image in image_itr:
                    source_images.append(StationImage(image, '../..'))
            if source_images is not None and (not len(source_images) > 0):
                source_images = None

            if source_images is not None:
                images.append({
                    'name': source.source_name,
                    'description': source.description,
                    'images': source_images
                })
    return images
