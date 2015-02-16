# coding=utf-8
from datetime import timedelta, datetime
from pytz import timezone
from twisted.python import log

__author__ = 'david'

# TODO: this entire class
class DstInfo(object):
    """
    Provides access to daylight savings transition times for a particular
    timezone.
    """

    _half_year = timedelta(182)

    def __init__(self, time_zone, time_source=datetime.now):
        """
        Creates a new instance of the DstInfo class. It returns the nearest
        daylight savings transition points to the current time. This means that
        for around six months the dst_end property will return the _last_
        dst end time, then for the next ~6 months it will return the _next_
        dst end time.

        Note that this will not work with a two hour (120 minute) station
        archive interval. Stations with this archive interval must use automatic
        daylight savings. If the station is in a region where automatic daylight
        savings is not supported then the user must choose a sensible archive
        interval like 5 or 10 minutes. This is fine because archive intervals
        above 15 minutes aren't likely to work well with zxweather anyway. An
        interval of 120 minutes would make for some very empty looking graphs.

        :param time_zone: The timezone to supply information for. This should
        be of the form Pacific/Auckland. You can find a list of timezones on
        Wikipedia at http://en.wikipedia.org/wiki/List_of_tz_database_time_zones
        :type time_zone: str
        :param time_source: Function that returns the current datetime. By
        default this is datetime.now(). This parameter exists only to allow for
        unit testing.
        """

        self._now = time_source

        self._tz = timezone(time_zone)

        self._load_transitions()

        log.msg("Nearest DST Start: {0}".format(self.dst_start))
        log.msg("Nearest DST End: {0}".format(self.dst_end))

    def _load_transitions(self):

        self._start_transitions = []
        self._end_transitions = []

        for index, time in enumerate(self._tz._utc_transition_times):

            transition_info = self._tz._transition_info[index]

            utc_offset = transition_info[0]
            standard_offset = transition_info[1]
            zone = transition_info[2]

            # Time is in UTC so we'll adjust it to local
            time += utc_offset

            if standard_offset.seconds == 0:
                # This is the DST end transition
                time += self.dst_offset
                transition = (time, zone, standard_offset, utc_offset)
                self._end_transitions.append(transition)
            else:
                # The DST Start transition
                time -= self.dst_offset
                transition = (time, zone, standard_offset, utc_offset)
                self._start_transitions.append(transition)

    def _nearest_dst_start_transition(self):
        while True:
            if len(self._start_transitions) == 0:
                raise Exception("Daylight savings not available for this "
                                "timezone")

            ts = self._start_transitions[0][0]
            if self._now() - self._half_year > ts:
                # The transition was more than half a year ago. Don't care about
                # it.
                self._start_transitions.pop(0)
            else:
                return self._start_transitions[0]

    def _nearest_dst_end_transition(self):
        while True:
            if len(self._end_transitions) == 0:
                raise Exception("Daylight savings not available for this "
                                "timezone")

            ts = self._end_transitions[0][0]
            if self._now() - self._half_year > ts:
                # The transition was more than half a year ago. Don't care about
                # it.
                self._end_transitions.pop(0)
            else:
                return self._end_transitions[0]

    @property
    def dst_offset(self):
        """
        Returns the daylight savings offset
        :rtype: timedelta
        """

        return timedelta(0, 3600)

    @property
    def dst_timezone(self):
        """
        Returns the time zone code (eg, NZDT) used during daylight savings time
        """
        transition = self._nearest_dst_start_transition()

        return transition[3].seconds/60  # offset from UTC in minutes

    @property
    def standard_timezone(self):
        """
        Returns the time zone code (eg, NZST) used when daylight savings is not
        in effect.
        """
        transition = self._nearest_dst_end_transition()

        return transition[3].seconds / 60  # offset from UTC in minutes

    @property
    def dst_start(self):
        """
        Returns the nearest daylight savings start time. This could be up to
        half a year in the past or half a year into the future depending
        """
        transition = self._nearest_dst_start_transition()

        return transition[0]

    @property
    def dst_end(self):
        """
        Returns the nearest daylight savings end time. This could be up to
        half a year in the past or half a year into the future depending
        """
        transition = self._nearest_dst_end_transition()

        return transition[0]


def _get_timestamp(sample):
    return datetime.combine(sample.dateStamp, sample.timeStamp)


class NullDstSwitcher(object):
    """
    A DST Switching class that doesn't switch DST. Used for when automatic
    daylight savings is turned off.
    """

    def process_sample(self, sample):
        """
        Does nothing
        :param sample: Input sample
        :type sample: record_types.dmp.Dmp
        :returns: The input sample
        :rtype: record_types.dmp.Dmp
        """
        return sample

    @property
    def station_time_needs_adjusting(self):
        """
        Does nothing
        :returns: False
        :rtype: bool
        """
        return False

    @property
    def new_dst_state_is_on(self):
        """
        Does nothing
        :returns: None
        :rtype: None
        """
        return None

    def suppress_dst_off(self):
        """
        Does nothing
        """
        pass


class DstSwitcher(NullDstSwitcher):
    """
    Handles switching daylight savings on and off.
    """

    def __init__(self, dst_info, sample_interval, previous_timestamp):
        """
        :param dst_info: Daylight savings information
        :type dst_info: DstInfo
        :param sample_interval: Stations archive period
        :type sample_interval: int
        :param previous_timestamp: The last sample timestamp recorded for the
        station in the database.
        :type previous_timestamp: datetime
        """
        self._dst_info = dst_info
        self._sample_interval = timedelta(0, sample_interval * 60)
        self._previous_timestamp = previous_timestamp.replace(tzinfo=None)

        self._switch_mode = False
        self._fix_samples = False
        self._dst_on = None

        # This is set to True only in the constructor. It triggers some
        # additional checks to be performed on the first sample to see if
        # there was a daylight savings transition between the first sample and
        # the last sample logged
        self._init = True

        # This is set to True when DST is turned off to prevent DST Off
        # being triggered when it sees the DST on timestamp repeated.
        # It is set back to False next time daylight savings is turned on
        # or the application is restarted
        self._suppress_dst_off = False

    def _log(self, msg):
        log.msg(msg)

    def _adjust_sample(self, sample):

        time_stamp = _get_timestamp(sample)

        expected_next_ts = self._previous_timestamp + self._sample_interval

        # Allow for manual corrections to be up to 5 minutes out
        margin = timedelta(0, 300)
        max_skip = expected_next_ts + (self._dst_info.dst_offset - margin)

        if time_stamp < self._previous_timestamp and self._dst_on is False:
            # Station clock has gone back in time and we were planning on
            # turning daylight savings off. I guess the user has already
            # done it manually. Don't need to adjust samples anymore.

            self._log("Station time appears to have gone back. Assuming DST has"
                      "been turned off on station. Switching off DST fix mode.")

            self._dst_on = None
            self._switch_mode = False

            return sample

        elif time_stamp > max_skip and self._dst_on is True:
            # The clock has jumped ahead by nearly an hour. We were planning
            # on adjusting it forward by an hour to turn daylight savings
            # on. I guess the user has already done it manually.

            self._log("Station time appears to have gone forward by nearly an "
                      "hour. Assuming DST has been turned on on station. "
                      "Switching off DST fix mode.")

            self._dst_on = None
            self._switch_mode = False

            return sample

        # Looks like we still need to adjust some timestamps.
        if self._dst_on is True:
            new_ts = time_stamp + self._dst_info.dst_offset

            sample.timeStamp = new_ts.time()
            sample.timeZone = self._dst_info.dst_timezone
        else:
            new_ts = time_stamp - self._dst_info.dst_offset
            sample.timeStamp = new_ts.time()
            sample.timeZone = self._dst_info.standard_timezone

        return sample

    def _enter_dst_mode(self, dst_on, time_stamp):
        # DST Mode has been switched!
        self._switch_mode = True

        # DST is being turned ON
        self._dst_on = dst_on

        # Only allow DST OFF to be detected if the last transition was DST ON
        self._suppress_dst_off = not dst_on

        # Store the timestamp so we can detect changes to the station clock
        # made by the user.
        self._previous_timestamp = time_stamp

    def _set_sample_timezone(self, sample):

        if self._switch_mode:
            # The samples timezone will have been set as part of the time
            # correction. Nothing to do here.
            return sample

        time_stamp = _get_timestamp(sample)

        one_hour = timedelta(0, 3600)
        dst_end = self._dst_info.dst_end

        if dst_end - one_hour <= time_stamp < dst_end:
            # We're in the DST End transition hour. Each time in this hour will
            # occur twice due to the clock being put back. As a result we need
            # to each sample has a timezone set so they're unambiguous when they
            # hit the database.

            if self._suppress_dst_off:
                # This variable is set when we pass the DST End transition point
                # (or we're told that we've passed it) to prevent the repeat
                # occurrence of the transition time being treated as a real
                # transition. So if this variable is set we know DST has been
                # turned off one way or another.
                sample.timeZone = self._dst_info.standard_timezone
            else:
                sample.timeZone = self._dst_info.dst_timezone

        return sample

    def process_sample(self, sample):
        """
        Processes a single sample possibly altering its timestamp to account for
        daylight savings
        :param sample: Input sample
        :type sample: record_types.dmp.Dmp
        :returns: Corrected sample
        :rtype: record_types.dmp.Dmp
        """

        time_stamp = _get_timestamp(sample)

        expected_next_ts = time_stamp + self._sample_interval

        if self._switch_mode:
            # DST has been switched on or off by a previous sample. All incoming
            # samples need their timestamps and timezones adjusted.
            sample = self._adjust_sample(sample)
            self._previous_timestamp = time_stamp

        elif time_stamp < self._dst_info.dst_start <= expected_next_ts:
            # Daylight savings starts between now and when the next sample is
            # due to be logged. Time for DST Mode!

            self._log("Turning on DST fix mode. DST now ON. Sample not "
                      "modified.")

            self._enter_dst_mode(True, time_stamp)

        elif time_stamp < self._dst_info.dst_end <= expected_next_ts:
            # Daylight savings ends between now and when the next sample is due
            # to be logged. Time for DST Mode!

            if self._suppress_dst_off:
                self._log("DST OFF trigger ignored - previous transition was "
                          "DST OFF")
            else:
                self._log("Turning on DST fix mode. DST now OFF. Sample not "
                          "modified.")

                # Set the timezone here as the call later in the function won't
                # do it once _enter_dst_mode has turned fix mode on.
                sample = self._set_sample_timezone(sample)

                self._enter_dst_mode(False, time_stamp)


        elif self._init and self._previous_timestamp < self._dst_info.dst_start <= time_stamp:
            # self._init is set to True only by the constructor. If we get
            # in here then we are processing our first sample for the day and
            # daylight savings started sometime between this sample and the
            # previous recorded sample. We need to start fixing samples from
            # this sample onwards.
            self._log("DST starts NOW. Turning on DST fix mode and fixing "
                      "sample.")
            self._enter_dst_mode(True, time_stamp)
            sample = self._adjust_sample(sample)
            self._previous_timestamp = time_stamp

        elif self._init and self._previous_timestamp < self._dst_info.dst_end <= time_stamp:
            # self._init is set to True only by the constructor. If we get
            # in here then we are processing our first sample for the day and
            # daylight savings finished. We need to start fixing samples from
            # this sample onwards.

            if self._suppress_dst_off:
                self._log("DST OFF trigger ignored - previous transition was "
                          "DST OFF")
            else:
                self._log("DST ends NOW. Turning on DST fix mode and fixing "
                          "sample.")
                self._enter_dst_mode(False, time_stamp)
                sample = self._adjust_sample(sample)
                self._previous_timestamp = time_stamp

        self._init = False

        return self._set_sample_timezone(sample)

    @property
    def station_time_needs_adjusting(self):
        """
        Returns true if the station needs its clock adjusted by turning daylight
        savings on or off
        """
        return self._switch_mode

    @property
    def new_dst_state_is_on(self):
        """
        Returns the new daylight savings state. If station_time_needs_adjusting
        is False then this will return None.
        """
        return self._dst_on

    def suppress_dst_off(self):
        """
        Call this on a new instance of the DstSwitcher if the data logger is
        started within the hour following DST being turned off. This will
        prevent DST from being turned off a second time when the time DST is
        normally turned off passes a second time (due to DST being put back
        an hour)
        """
        self._suppress_dst_off = True
