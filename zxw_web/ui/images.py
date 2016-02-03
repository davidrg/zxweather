# coding=utf-8
import mimetypes

from database import get_image_sources_for_station, get_images_for_source


class StationImage(object):
    _img_url_fmt = '/data/{station_code}/{year}/{month}/{day}/images/' \
                   '{source_code}/{image_id}/{type}{extension}'

    def __init__(self, img_data, url_prefix=""):
        self._time_stamp = img_data.time_stamp
        self._mime = img_data.mime_type
        self._id = img_data.id
        self._src = img_data.source
        self._station = img_data.station
        self._url_prefix = url_prefix

        self._extension = mimetypes.guess_extension(self._mime)
        if self._extension == ".jpe":
            self._extension = ".jpeg"

        self._day = self._time_stamp.date()

    @property
    def thumbnail_url(self):
        return self._url_prefix + self._img_url_fmt.format(
            station_code=self._station.lower(),
            year=self._day.year,
            month=self._day.month,
            day=self._day.day,
            source_code=self._src.lower(),
            image_id=self._id,
            type="thumbnail",
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
            image_id=self._id,
            type="full",
            extension=self._extension
        )

    @property
    def time(self):
        return self._time_stamp


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
            if source_images is not None and not len(source_images):
                source_images = None

            if source_images is not None:
                images.append({
                    'name': source.source_name,
                    'description': source.description,
                    'images': source_images
                })
    return images
