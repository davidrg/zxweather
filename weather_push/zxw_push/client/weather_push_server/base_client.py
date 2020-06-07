
# TODO: move as much common code out of WeatherPushProtocol and WeatherPushDatagramClient
#       as possible into here.

class WeatherPushProtocolBase(object):
    _STATION_LIST_TIMEOUT = 60  # seconds to wait for a response

    def __init__(self, authorisation_code, confirmed_sample_func):
        self._authorisation_code = authorisation_code
        self._confirmed_sample_func = confirmed_sample_func
