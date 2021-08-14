from datetime import date, time, datetime, timedelta
import unittest
from davis_logger.dst_switcher import DstSwitcher, DstInfo
from davis_logger.record_types.dmp import Dmp

__author__ = 'david'

class FakeDstInfo(DstInfo):

    def __init__(self):
        super(FakeDstInfo, self).__init__("Pacific/Auckland")

    @property
    def dst_offset(self):
        return timedelta(0, 3600)  # One hour

    @property
    def dst_timezone(self):
        return "+13"

    @property
    def standard_timezone(self):
        return "+12"

    @property
    def dst_start(self):
        return datetime(2015, 9, 27, 2, 0, 0)

    @property
    def dst_end(self):
        return datetime(2015, 4, 5, 3, 0, 0)


class TestDmp(Dmp):
    """
    A utility Dmp class to make the unit tests easier to write. Just takes
    a timestamp and timezone as a parameter and nothing else.
    """
    def __init__(self, ts, tz):

        date_stamp = ts.date()
        time_stamp = ts.time()
        time_zone = tz

        super(TestDmp, self).__init__(date_stamp, time_stamp, time_zone,
                                      None, None, None, None, None, None,
                                      None, None, None, None, None, None,
                                      None, None, None, None, None, None,
                                      None, None, None, None, None, None,
                                      None, None, None, None)


class DstSwitcherTests(unittest.TestCase):

    def test_sample_is_returned(self):
        """
        Checks the DstSwither returns a sample
        """

        sample = TestDmp(datetime(2015, 1, 5, 5, 20, 0), 'a')
        dst_info = FakeDstInfo()

        switcher = DstSwitcher(dst_info, 5, datetime.now())

        result = switcher.process_sample(sample)

        self.assertIsNotNone(result)

    def _make_samples(self, start_ts, interval, tz, count):

        delta = timedelta(0, interval * 60)

        samples = []

        ts = start_ts

        for i in range(0, count):
            samples.append(TestDmp(ts, tz))
            ts += delta

        return samples

    def test_samples_are_unaltered_when_no_dst_switch(self):
        """
        Tests that samples pass through the DstSwitcher unaltered when we don't
        cross a DST switch threshold.
        """

        start_ts = datetime(2015, 1, 5, 5, 20, 0)

        samples = self._make_samples(start_ts, 5, None, 10)

        # We're not expecting the output to be any different from the input.
        expected_samples = list(samples)

        sample_interval = 5

        dst_info = FakeDstInfo()

        previous_ts = start_ts - timedelta(0, sample_interval * 60)

        switcher = DstSwitcher(dst_info, sample_interval, previous_ts)

        for index, sample in enumerate(samples):
            adjusted_sample = switcher.process_sample(sample)
            expected_sample = expected_samples[index]

            self.assertIsNotNone(adjusted_sample)

            expected_ts = datetime.combine(expected_sample.dateStamp,
                                           expected_sample.timeStamp)
            output_ts = datetime.combine(adjusted_sample.dateStamp,
                                         adjusted_sample.timeStamp)

            self.assertEqual(expected_ts, output_ts)
            self.assertEqual(adjusted_sample.timeZone, expected_sample.timeZone)

        self.assertFalse(switcher.station_time_needs_adjusting)
        self.assertIsNone(switcher.new_dst_state_is_on)

    def test_samples_have_timestamp_fixed_on_other_side_of_dst_start(self):
        """
        Runs four samples through the DstSwitcher with two on each side of
        the daylight savings start point. The second pair of samples (after the
        switch point) should have their timestamps put forward by one hour.
        """

        dst_info = FakeDstInfo()

        sample_interval = 5
        sample_interval_seconds = sample_interval * 60

        start_ts = dst_info.dst_start - timedelta(0, sample_interval_seconds*2)
        prev_ts = start_ts - timedelta(0, sample_interval_seconds)

        print("Start TS: {0}".format(start_ts))

        samples = self._make_samples(start_ts, sample_interval,
                                     dst_info.standard_timezone, 4)

        switcher = DstSwitcher(dst_info, sample_interval, prev_ts)

        for index, sample in enumerate(samples):
            input_ts = datetime.combine(sample.dateStamp, sample.timeStamp)

            adjusted = switcher.process_sample(sample)

            output_ts = datetime.combine(adjusted.dateStamp, adjusted.timeStamp)

            print("{0}\tIn: {1}\tOut: {2}".format(index, input_ts, output_ts))

            if index <= 1:
                # First two samples shouldn't have their timestamp altered.
                self.assertEqual(input_ts, output_ts)
            else:
                # Second two samples should be put forward by the dst offset
                self.assertEqual(input_ts, output_ts - dst_info.dst_offset)

        self.assertTrue(switcher.station_time_needs_adjusting)
        self.assertTrue(switcher.new_dst_state_is_on)

    def test_samples_have_timestamp_fixed_on_other_side_of_dst_finish(self):
        """
        Runs four samples through the DstSwitcher with two on each side of
        the daylight savings finish point. The second pair of samples (after the
        switch point) should have their timestamps put backwards by one hour.
        """

        dst_info = FakeDstInfo()

        sample_interval = 5
        sample_interval_seconds = sample_interval * 60

        start_ts = dst_info.dst_end - timedelta(0, sample_interval_seconds*2)
        prev_ts = start_ts - timedelta(0, sample_interval_seconds)

        print("Start TS: {0}".format(start_ts))

        samples = self._make_samples(start_ts, sample_interval,
                                     dst_info.dst_timezone, 4)

        switcher = DstSwitcher(dst_info, sample_interval, prev_ts)

        for index, sample in enumerate(samples):
            input_ts = datetime.combine(sample.dateStamp, sample.timeStamp)

            adjusted = switcher.process_sample(sample)

            output_ts = datetime.combine(adjusted.dateStamp, adjusted.timeStamp)

            print("{0}\tIn: {1}\tOut: {2}".format(index, input_ts, output_ts))

            if index <= 1:
                # First two samples shouldn't have their timestamp altered.
                self.assertEqual(input_ts, output_ts)
            else:
                # Second two samples should be put backwards by the dst offset
                self.assertEqual(input_ts, output_ts + dst_info.dst_offset)

        self.assertTrue(switcher.station_time_needs_adjusting)
        self.assertFalse(switcher.new_dst_state_is_on)

    def test_samples_after_dst_finish_are_not_double_fixed(self):
        """
        Runs the following timestamps through the switcher:
          250 255 300 305 310 315 320 325 330 335 340 345 350 355 400 405
        DST ends at 300 where the clock should be put back resulting in this:
          250 255 200 205 210 215 220 225 230 235 240 245 250 255 300 305
        This test specifically checks that the second instance of 300 doesn't
        cause any funny behaviour like applying DST twice
        """

        dst_info = FakeDstInfo()

        sample_interval = 5
        sample_interval_seconds = sample_interval * 60

        start_ts = dst_info.dst_end - timedelta(0, sample_interval_seconds*2)
        prev_ts = start_ts - timedelta(0, sample_interval_seconds)

        print("Start TS: {0}".format(start_ts))

        samples = self._make_samples(start_ts, sample_interval,
                                     dst_info.dst_timezone, 16)

        # This is what we expect to get after the DST switcher has adjusted
        # samples
        expected_result = self._make_samples(
            dst_info.dst_end - timedelta(0, 3600), sample_interval,
            dst_info.standard_timezone, 14)
        pre_switch_samples = [
            TestDmp(dst_info.dst_end - timedelta(0, 600), dst_info.dst_timezone),
            TestDmp(dst_info.dst_end - timedelta(0, 300), dst_info.dst_timezone)
        ]
        expected_samples = pre_switch_samples + expected_result

        # And these are the timestamps from above
        expected_result = [(datetime.combine(x.dateStamp, x.timeStamp),
                            x.timeZone) for x in expected_samples]

        switcher = DstSwitcher(dst_info, sample_interval, prev_ts)

        output_samples = [switcher.process_sample(sample) for sample in samples]

        # Throw away the Dmp class wrappings and just build a list of timestamps
        result = [(datetime.combine(x.dateStamp, x.timeStamp),
                   x.timeZone) for x in output_samples]

        self.maxDiff = None
        self.assertListEqual(expected_result, result)
        self.assertTrue(switcher.station_time_needs_adjusting)
        self.assertFalse(switcher.new_dst_state_is_on)

    def test_dst_start_is_first_sample(self):
        """
        Tests that when the very first sample the DST Switcher sees is the start
        point for daylight savings it still correctly starts daylight savings.
        For example, for the input:
          200 205 210 215
        we're expecting the output:
          300 305 310 315
        where the DST start time is 200
        """
        dst_info = FakeDstInfo()

        sample_interval = 5
        sample_interval_seconds = sample_interval * 60

        start_ts = dst_info.dst_start
        prev_ts = start_ts - timedelta(0, sample_interval_seconds)

        print("Start TS: {0}".format(start_ts))

        samples = self._make_samples(start_ts, sample_interval,
                                     dst_info.standard_timezone, 4)

        switcher = DstSwitcher(dst_info, sample_interval, prev_ts)

        for index, sample in enumerate(samples):
            input_ts = datetime.combine(sample.dateStamp, sample.timeStamp)

            adjusted = switcher.process_sample(sample)

            output_ts = datetime.combine(adjusted.dateStamp, adjusted.timeStamp)

            print("{0}\tIn: {1}\tOut: {2}".format(index, input_ts, output_ts))

            self.assertEqual(input_ts, output_ts - dst_info.dst_offset)

        self.assertTrue(switcher.station_time_needs_adjusting)
        self.assertTrue(switcher.new_dst_state_is_on)

    def test_dst_end_is_first_sample(self):
        """
        Tests that when the very first sample the DST Switcher sees is the end
        point for daylight savings it still correctly ends daylight savings.
        For example, for the input:
          300 305 310 315
        we're expecting the output:
          200 205 210 215
        where the DST end time is 300
        """
        dst_info = FakeDstInfo()

        sample_interval = 5
        sample_interval_seconds = sample_interval * 60

        start_ts = dst_info.dst_end
        prev_ts = start_ts - timedelta(0, sample_interval_seconds)

        print("Start TS: {0}".format(start_ts))

        samples = self._make_samples(start_ts, sample_interval,
                                     dst_info.standard_timezone, 4)

        switcher = DstSwitcher(dst_info, sample_interval, prev_ts)

        for index, sample in enumerate(samples):
            input_ts = datetime.combine(sample.dateStamp, sample.timeStamp)

            adjusted = switcher.process_sample(sample)

            output_ts = datetime.combine(adjusted.dateStamp, adjusted.timeStamp)

            print("{0}\tIn: {1}\tOut: {2}".format(index, input_ts, output_ts))

            self.assertEqual(input_ts, output_ts + dst_info.dst_offset)

        self.assertTrue(switcher.station_time_needs_adjusting)
        self.assertFalse(switcher.new_dst_state_is_on)

    def test_last_sample_borders_dst_start_no_samples_altered(self):
        """
        When DST starts at 200 and the input samples are:
          140 145 150 155
        no samples should be altered
        """
        dst_info = FakeDstInfo()

        sample_interval = 5
        sample_interval_seconds = sample_interval * 60

        start_ts = dst_info.dst_start - timedelta(0, 4*sample_interval_seconds)
        prev_ts = start_ts - timedelta(0, sample_interval_seconds)

        print("Start TS: {0}".format(start_ts))

        samples = self._make_samples(start_ts, sample_interval,
                                     dst_info.standard_timezone, 4)

        switcher = DstSwitcher(dst_info, sample_interval, prev_ts)

        for index, sample in enumerate(samples):
            input_ts = datetime.combine(sample.dateStamp, sample.timeStamp)

            adjusted = switcher.process_sample(sample)

            output_ts = datetime.combine(adjusted.dateStamp, adjusted.timeStamp)

            print("{0}\tIn: {1}\tOut: {2}".format(index, input_ts, output_ts))

            self.assertEqual(input_ts, output_ts)

        # The very next sample will need fixing so station time should be
        # flagged as needing adjustment
        self.assertTrue(switcher.station_time_needs_adjusting)
        self.assertTrue(switcher.new_dst_state_is_on)

    def test_last_sample_borders_dst_end_no_samples_altered(self):
        """
        When DST ends at 300 and the input samples are:
          240 245 250 255
        no samples should be altered. Instead DstSwitcher should assume that
        the station clock has already been adjusted.
        """
        dst_info = FakeDstInfo()

        sample_interval = 5
        sample_interval_seconds = sample_interval * 60

        start_ts = dst_info.dst_end - timedelta(0, 4*sample_interval_seconds)
        prev_ts = start_ts - timedelta(0, sample_interval_seconds)

        print("Start TS: {0}".format(start_ts))

        samples = self._make_samples(start_ts, sample_interval,
                                     dst_info.standard_timezone, 4)

        switcher = DstSwitcher(dst_info, sample_interval, prev_ts)

        for index, sample in enumerate(samples):
            input_ts = datetime.combine(sample.dateStamp, sample.timeStamp)

            adjusted = switcher.process_sample(sample)

            output_ts = datetime.combine(adjusted.dateStamp, adjusted.timeStamp)

            print("{0}\tIn: {1}\tOut: {2}".format(index, input_ts, output_ts))

            self.assertEqual(input_ts, output_ts)

        # The very next sample will need fixing so station time should be
        # flagged as needing adjustment
        self.assertTrue(switcher.station_time_needs_adjusting)
        self.assertFalse(switcher.new_dst_state_is_on)

    def test_dst_off_is_detected_and_fix_mode_is_turned_off(self):
        """
        Tests that the situation where the user or data logger puts the station
        clock back after the DST end switch point is handled correctly
        (DstSwitcher should only alter samples up to the point where the clock
        was manually adjusted and station_time_needs_adjusting should report
        False at the end)
        """

        dst_info = FakeDstInfo()

        sample_interval = 5
        sample_interval_seconds = sample_interval * 60

        start_ts = dst_info.dst_end - timedelta(0, sample_interval_seconds*2)
        prev_ts = start_ts - timedelta(0, sample_interval_seconds)

        # This will be fed into the switcher first
        pre_samples = self._make_samples(start_ts, sample_interval,
                                         dst_info.dst_timezone, 17)
        pre_expected = self._make_samples(
            dst_info.dst_end - timedelta(0, 3600), sample_interval,
            dst_info.standard_timezone, 15)
        pre_switch_samples = [
            TestDmp(dst_info.dst_end - timedelta(0, 600), dst_info.dst_timezone),
            TestDmp(dst_info.dst_end - timedelta(0, 300), dst_info.dst_timezone)
        ]
        pre_expected = pre_switch_samples + pre_expected
        pre_samples_ts = [(datetime.combine(x.dateStamp, x.timeStamp),
                           x.timeZone) for x in pre_samples]
        pre_expected_ts = [(datetime.combine(x.dateStamp, x.timeStamp),
                            x.timeZone) for x in pre_expected]

        # Then we'll pretend to fix the weather stations clock and send through
        # these samples which *DONT* need DST fixed. So they should pass through
        # the switcher unmodified
        post_start = dst_info.dst_end + timedelta(0, 3*sample_interval)
        post_samples = self._make_samples(post_start, sample_interval,
                                          dst_info.dst_timezone, 12)
        post_expected = list(post_samples)
        post_samples_ts = [(datetime.combine(x.dateStamp, x.timeStamp),
                            x.timeZone) for x in post_samples]
        post_expected_ts = [(datetime.combine(x.dateStamp, x.timeStamp),
                             x.timeZone) for x in post_expected]

        switcher = DstSwitcher(dst_info, sample_interval, prev_ts)

        pre_output = [switcher.process_sample(sample) for sample in pre_samples]
        pre_output_ts = [(datetime.combine(x.dateStamp, x.timeStamp),
                          x.timeZone) for x in pre_output]

        # Ok. We've run a bunch of samples through. The DstSwitcher should have
        # adjusted for daylight savings ending on all the samples that need it
        # and it should report that the station time needs adjusting

        self.maxDiff = None
        self.assertListEqual(pre_expected_ts, pre_output_ts)
        self.assertTrue(switcher.station_time_needs_adjusting)
        self.assertFalse(switcher.new_dst_state_is_on)

        # We will adjust the stations clock and run a bunch more samples
        # through the DST switcher. The switcher should see that the timestamp
        # has jumped back and know that it doesn't need to fix up samples
        # anymore. These samples should pass through unmodified.

        post_output = [switcher.process_sample(sample) for sample in post_samples]
        post_output_ts = [(datetime.combine(x.dateStamp, x.timeStamp),
                          x.timeZone) for x in post_output]

        self.assertListEqual(post_expected_ts, post_output_ts)
        self.assertFalse(switcher.station_time_needs_adjusting)

    def test_dst_on_is_detected_and_fix_mode_is_turned_off(self):
        """
        Tests that the situation where the user or data logger puts the station
        clock forward after the DST start switch point is handled correctly
        (DstSwitcher should only alter samples up to the point where the clock
        was manually adjusted and station_time_needs_adjusting should report
        False at the end)
        """

        dst_info = FakeDstInfo()

        sample_interval = 5
        sample_interval_seconds = sample_interval * 60

        start_ts = dst_info.dst_start - timedelta(0, sample_interval_seconds*2)
        prev_ts = start_ts - timedelta(0, sample_interval_seconds)

        # This will be fed into the switcher first
        pre_samples = self._make_samples(start_ts, sample_interval,
                                         dst_info.standard_timezone, 17)
        pre_expected = self._make_samples(
            dst_info.dst_start + dst_info.dst_offset,
            sample_interval, dst_info.dst_timezone, 15)
        pre_switch_samples = [
            TestDmp(dst_info.dst_start - timedelta(0, 600), dst_info.standard_timezone),
            TestDmp(dst_info.dst_start - timedelta(0, 300), dst_info.standard_timezone)
        ]
        pre_expected = pre_switch_samples + pre_expected
        pre_samples_ts = [(datetime.combine(x.dateStamp, x.timeStamp),
                           x.timeZone) for x in pre_samples]
        pre_expected_ts = [(datetime.combine(x.dateStamp, x.timeStamp),
                            x.timeZone) for x in pre_expected]

        # Then we'll pretend to fix the weather stations clock and send through
        # these samples which *DONT* need DST fixed. So they should pass through
        # the switcher unmodified
        post_start = datetime.combine(pre_samples[-1].dateStamp,
                                      pre_samples[-1].timeStamp)
        post_start += timedelta(0, sample_interval_seconds)
        post_start += dst_info.dst_offset
        post_samples = self._make_samples(post_start, sample_interval,
                                          dst_info.dst_timezone, 12)
        post_expected = list(post_samples)
        post_samples_ts = [(datetime.combine(x.dateStamp, x.timeStamp),
                            x.timeZone) for x in post_samples]
        post_expected_ts = [(datetime.combine(x.dateStamp, x.timeStamp),
                             x.timeZone) for x in post_expected]

        switcher = DstSwitcher(dst_info, sample_interval, prev_ts)

        pre_output = [switcher.process_sample(sample) for sample in pre_samples]
        pre_output_ts = [(datetime.combine(x.dateStamp, x.timeStamp),
                          x.timeZone) for x in pre_output]

        # Ok. We've run a bunch of samples through. The DstSwitcher should have
        # adjusted for daylight savings ending on all the samples that need it
        # and it should report that the station time needs adjusting

        self.maxDiff = None
        self.assertListEqual(pre_expected_ts, pre_output_ts)
        self.assertTrue(switcher.station_time_needs_adjusting)
        self.assertTrue(switcher.new_dst_state_is_on)

        # We will adjust the stations clock and run a bunch more samples
        # through the DST switcher. The switcher should see that the timestamp
        # has jumped forward and know that it doesn't need to fix up samples
        # anymore. These samples should pass through unmodified.

        post_output = [switcher.process_sample(sample) for sample in post_samples]
        post_output_ts = [(datetime.combine(x.dateStamp, x.timeStamp),
                          x.timeZone) for x in post_output]

        self.assertListEqual(post_expected_ts, post_output_ts)
        self.assertFalse(switcher.station_time_needs_adjusting)

    def test_clock_put_forward_on_dst_start_point(self):
        """
        Tests that the situation where the user manually (or the station itself
        due to automatic daylight savings being on) puts the station clock
        forward exactly on the DST start switch point is correctly handled
        (DST Switcher should not alter any records)
        """
        dst_info = FakeDstInfo()

        sample_interval = 5
        sample_interval_seconds = sample_interval * 60

        prev_ts = dst_info.dst_start - timedelta(0, 3 * sample_interval_seconds)

        samples = self._make_samples(dst_info.dst_start + dst_info.dst_offset,
                                     sample_interval,
                                     dst_info.dst_timezone, 4)
        pre_samples = [
            TestDmp(dst_info.dst_start - timedelta(0, 600),
                    dst_info.standard_timezone),
            TestDmp(dst_info.dst_start - timedelta(0, 300),
                    dst_info.standard_timezone)
        ]
        samples = pre_samples + samples

        # This is what we expect to get after the DST switcher has adjusted
        # samples. That is, it shouldn't adjust samples.
        expected_samples = list(samples)

        # And these are the timestamps from above
        expected_result = [(datetime.combine(x.dateStamp, x.timeStamp),
                            x.timeZone) for x in expected_samples]

        switcher = DstSwitcher(dst_info, sample_interval, prev_ts)

        output_samples = [switcher.process_sample(sample) for sample in samples]

        # Throw away the Dmp class wrappings and just build a list of timestamps
        result = [(datetime.combine(x.dateStamp, x.timeStamp),
                   x.timeZone) for x in output_samples]

        self.maxDiff = None
        self.assertListEqual(expected_result, result)
        self.assertFalse(switcher.station_time_needs_adjusting)

    def test_clock_put_backward_on_dst_end_point(self):
        """
        Tests that the situation where the user manually (or the station itself
        due to automatic daylight savings being on) puts the station clock
        back exactly on the DST end switch point is correctly handled
        (DST Switcher should not alter any records)
        """
        dst_info = FakeDstInfo()

        sample_interval = 5
        sample_interval_seconds = sample_interval * 60

        prev_ts = dst_info.dst_end - timedelta(0, 3 * sample_interval_seconds)

        samples = self._make_samples(dst_info.dst_end - dst_info.dst_offset,
                                     sample_interval,
                                     dst_info.standard_timezone, 4)
        pre_samples = [
            TestDmp(dst_info.dst_end - timedelta(0, 600), dst_info.dst_timezone),
            TestDmp(dst_info.dst_end - timedelta(0, 300), dst_info.dst_timezone)
        ]
        samples = pre_samples + samples

        # This is what we expect to get after the DST switcher has adjusted
        # samples. That is, it shouldn't adjust samples.
        expected_samples = list(samples)

        # And these are the timestamps from above
        expected_result = [(datetime.combine(x.dateStamp, x.timeStamp),
                            x.timeZone) for x in expected_samples]

        switcher = DstSwitcher(dst_info, sample_interval, prev_ts)

        output_samples = [switcher.process_sample(sample) for sample in samples]

        # Throw away the Dmp class wrappings and just build a list of timestamps
        result = [(datetime.combine(x.dateStamp, x.timeStamp),
                   x.timeZone) for x in output_samples]

        self.maxDiff = None
        self.assertListEqual(expected_result, result)
        self.assertFalse(switcher.station_time_needs_adjusting)

    def test_dst_off_not_triggered_second_time_after_station_time_fixed(self):
        """
        When daylight savings ends the clock is put back an hour. This results
        in certain timestamps appearing twice. For example:
          150 155 200 205 210 .... 250 255 200 205 210 ... 250 255 300 305 310
        If the stations clock is adjusted at the second occurrence of 210 and
        dst fix mode is turned off in DST Switcher, DST switcher shouldn't
        turn fix mode back on when it sees the second 3am come up.
        """

        dst_info = FakeDstInfo()

        sample_interval = 5
        sample_interval_seconds = sample_interval * 60

        start_ts = dst_info.dst_end - timedelta(0, sample_interval_seconds*2)
        prev_ts = start_ts - timedelta(0, sample_interval_seconds)

        # This will be fed into the switcher first
        pre_samples = self._make_samples(start_ts, sample_interval,
                                         dst_info.dst_timezone, 10)
        pre_expected = self._make_samples(
            dst_info.dst_end - dst_info.dst_offset,
            sample_interval, dst_info.standard_timezone, 8)
        pre_switch_samples = [
            TestDmp(dst_info.dst_end - timedelta(0, 600),
                    dst_info.dst_timezone),
            TestDmp(dst_info.dst_end - timedelta(0, 300),
                    dst_info.dst_timezone)
        ]
        pre_expected = pre_switch_samples + pre_expected
        pre_samples_ts = [(datetime.combine(x.dateStamp, x.timeStamp),
                           x.timeZone) for x in pre_samples]
        pre_expected_ts = [(datetime.combine(x.dateStamp, x.timeStamp),
                            x.timeZone) for x in pre_expected]

        # Then we'll pretend to fix the weather stations clock and send through
        # these samples which *DONT* need DST fixed. So they should pass through
        # the switcher unmodified
        post_start = datetime.combine(pre_samples[-1].dateStamp,
                                      pre_samples[-1].timeStamp)
        post_start += timedelta(0, sample_interval_seconds)
        post_start -= dst_info.dst_offset
        post_samples = self._make_samples(post_start, sample_interval,
                                          dst_info.standard_timezone, 19)
        post_expected = list(post_samples)
        post_samples_ts = [(datetime.combine(x.dateStamp, x.timeStamp),
                            x.timeZone) for x in post_samples]
        post_expected_ts = [(datetime.combine(x.dateStamp, x.timeStamp),
                             x.timeZone) for x in post_expected]

        switcher = DstSwitcher(dst_info, sample_interval, prev_ts)

        pre_output = [switcher.process_sample(sample) for sample in pre_samples]
        pre_output_ts = [(datetime.combine(x.dateStamp, x.timeStamp),
                          x.timeZone) for x in pre_output]

        # Ok. We've run a bunch of samples through. The DstSwitcher should have
        # adjusted for daylight savings ending on all the samples that need it
        # and it should report that the station time needs adjusting

        self.maxDiff = None
        self.assertListEqual(pre_expected_ts, pre_output_ts)
        self.assertTrue(switcher.station_time_needs_adjusting)
        self.assertFalse(switcher.new_dst_state_is_on)

        print("Stage B")

        # We will adjust the stations clock and run a bunch more samples
        # through the DST switcher. The switcher should see that the timestamp
        # has jumped forward and know that it doesn't need to fix up samples
        # anymore. These samples should pass through unmodified.

        post_output = [switcher.process_sample(sample) for sample in post_samples]
        post_output_ts = [(datetime.combine(x.dateStamp, x.timeStamp),
                          x.timeZone) for x in post_output]

        self.assertListEqual(post_expected_ts, post_output_ts)
        self.assertFalse(switcher.station_time_needs_adjusting)

    def test_repeat_dst_off_suppression_is_reset_after_dst_on(self):
        """
        When The DST Switcher turns daylight savings OFF it ignores all future
        DST OFF transition points until it passes a DST ON transition point.
        This is because the DST OFF transition time occurs twice due to the
        clock being put back when it is hit the first time.

        This test makes sure that when it passes a DST ON point it resumes
        checking for DST OFF.
        """
        dst_info = FakeDstInfo()

        sample_interval = 5
        sample_interval_seconds = sample_interval * 60

        #############
        # DST OFF #1
        #############
        start_ts = dst_info.dst_end - timedelta(0, sample_interval_seconds*2)
        prev_ts = start_ts - timedelta(0, sample_interval_seconds)

        samples = self._make_samples(start_ts, sample_interval,
                                     dst_info.standard_timezone, 4)
        switchback_ts = datetime.combine(samples[-1].dateStamp,
                                         samples[-1].timeStamp)
        switchback_ts += timedelta(0, sample_interval_seconds)
        switchback_ts -= dst_info.dst_offset
        samples.append(TestDmp(switchback_ts, dst_info.standard_timezone))

        switcher = DstSwitcher(dst_info, sample_interval, prev_ts)

        for index, sample in enumerate(samples):
            input_ts = datetime.combine(sample.dateStamp, sample.timeStamp)

            adjusted = switcher.process_sample(sample)

            output_ts = datetime.combine(adjusted.dateStamp, adjusted.timeStamp)

            print("{0}\tIn: {1}\tOut: {2}".format(index, input_ts, output_ts))

            if index in [0, 1, 4]:
                # First two samples and the last one shouldn't have their
                # timestamp altered.
                self.assertEqual(input_ts, output_ts)
            else:
                # Second two samples should be put backwards by the dst offset
                self.assertEqual(input_ts, output_ts + dst_info.dst_offset)

        #############
        # DST ON
        #############
        start_ts = dst_info.dst_start - timedelta(0, sample_interval_seconds*2)

        samples = self._make_samples(start_ts, sample_interval,
                                     dst_info.standard_timezone, 4)

        switch_forward_ts = datetime.combine(samples[-1].dateStamp,
                                             samples[-1].timeStamp)
        switch_forward_ts += timedelta(0, sample_interval_seconds)
        switch_forward_ts += dst_info.dst_offset
        samples.append(TestDmp(switch_forward_ts, dst_info.dst_timezone))

        for index, sample in enumerate(samples):
            input_ts = datetime.combine(sample.dateStamp, sample.timeStamp)

            adjusted = switcher.process_sample(sample)

            output_ts = datetime.combine(adjusted.dateStamp, adjusted.timeStamp)

            print("{0}\tIn: {1}\tOut: {2}".format(index, input_ts, output_ts))

            if index in [0, 1, 4]:
                # First two samples shouldn't have their timestamp altered.
                self.assertEqual(input_ts, output_ts)
            else:
                # Second two samples should be put forward by the dst offset
                self.assertEqual(input_ts, output_ts - dst_info.dst_offset)

        #############
        # DST OFF #2
        #############
        start_ts = dst_info.dst_end - timedelta(0, sample_interval_seconds*2)

        samples = self._make_samples(start_ts, sample_interval,
                                     dst_info.standard_timezone, 4)

        for index, sample in enumerate(samples):
            input_ts = datetime.combine(sample.dateStamp, sample.timeStamp)

            adjusted = switcher.process_sample(sample)

            output_ts = datetime.combine(adjusted.dateStamp, adjusted.timeStamp)

            print("{0}\tIn: {1}\tOut: {2}".format(index, input_ts, output_ts))

            if index <= 1:
                # First two samples shouldn't have their timestamp altered.
                self.assertEqual(input_ts, output_ts)
            else:
                # Second two samples should be put backwards by the dst offset
                self.assertEqual(input_ts, output_ts + dst_info.dst_offset)

        # Check that its processed the DST OFF
        self.assertTrue(switcher.station_time_needs_adjusting)
        self.assertFalse(switcher.new_dst_state_is_on)

    def test_previous_sample_in_middle_of_dst_end_hour(self):
        """
        When daylight savings ends the clock is put back an hour. This results
        in certain timestamps appearing twice. For example:
          150 155 200 205 210 .... 250 255 200 205 210 ... 250 255 300 305 310
        If the data logger is stopped at the second occurrence of 210 (after
        daylight savings has been turned off) and then restarted at ten minutes
        later we need to make sure that when it sees 3am come up it ignores it
        as daylight savings was turned off last time the data logger was running
        """
        dst_info = FakeDstInfo()

        sample_interval = 5
        sample_interval_seconds = sample_interval * 60

        start_ts = dst_info.dst_end - timedelta(0, sample_interval_seconds*2)
        prev_ts = start_ts - timedelta(0, sample_interval_seconds)

        print("Start TS: {0}".format(start_ts))

        samples = self._make_samples(start_ts, sample_interval,
                                     dst_info.standard_timezone, 4)

        switcher = DstSwitcher(dst_info, sample_interval, prev_ts)

        # Tell the switcher DST is already turned off. Normally we'd check the
        # daylight savings setting on the weather station or look at the
        # database to detect this condition.
        switcher.suppress_dst_off()

        for index, sample in enumerate(samples):
            input_ts = datetime.combine(sample.dateStamp, sample.timeStamp)

            adjusted = switcher.process_sample(sample)

            output_ts = datetime.combine(adjusted.dateStamp, adjusted.timeStamp)

            print("{0}\tIn: {1}\tOut: {2}".format(index, input_ts, output_ts))

            # Samples should not be altered
            self.assertEqual(input_ts, output_ts)

        # The switcher shouldn't ask for DST to be turned off
        self.assertFalse(switcher.station_time_needs_adjusting)

    def test_timezone_set_for_hour_prior_to_dst_off(self):
        """
        For an hour prior to the daylight savings off transition point the DST
        Switcher should set the timezone on all samples to the DST timezone.
        This ensures that there is no confusion at the database layer between,
        for example, the timestamps 5-APR-2015 02:30 NZDT and
        5-APR-2015 02:30 NZST.
        """

        dst_info = FakeDstInfo()

        sample_interval = 5
        sample_interval_seconds = sample_interval*60

        start_ts = dst_info.dst_end - timedelta(0, 4200)  # 1 hour 10 minutes
        prev_ts = start_ts - timedelta(0, sample_interval_seconds)

        switcher = DstSwitcher(dst_info, sample_interval, prev_ts)

        samples = self._make_samples(start_ts, sample_interval, None, 14)

        result = [switcher.process_sample(sample) for sample in samples]

        result_timezones = [sample.timeZone for sample in result]

        expected_timezones = 12 * [dst_info.dst_timezone]

        # The first two samples shouldn't have a timezone set as they're more
        # than an hour prior to the transition point
        # noinspection PyTypeChecker
        expected_timezones = [None, None] + expected_timezones

        self.assertListEqual(result_timezones, expected_timezones)

    def test_timezone_set_for_hour_after_dst_off(self):
        """
        For an hour after the daylight savings off transition point the DST
        Switcher should set the timezone on all samples to the standard
        timezone. This ensures that there is no confusion at the database layer
        between, for example, the timestamps 5-APR-2015 02:30 NZDT and
        5-APR-2015 02:30 NZST.
        """

        dst_info = FakeDstInfo()

        sample_interval = 5
        sample_interval_seconds = sample_interval*60

        # Start TS is an hour after DST End to make the switcher thing the clock
        # has been put back. This will prevent it from 'fixing' samples ensuring
        # that any timezone set is not a result of the auto-fixing.
        start_ts = dst_info.dst_end - timedelta(0, 3600)

        # For this trick to work the previous TS needs to be just prior to the
        # transition point so that it can 'see' that the clock has gone back.
        prev_ts = dst_info.dst_end - timedelta(0, sample_interval_seconds)

        switcher = DstSwitcher(dst_info, sample_interval, prev_ts)

        switcher.suppress_dst_off()

        samples = self._make_samples(start_ts, sample_interval, None, 14)

        result = [switcher.process_sample(sample) for sample in samples]

        result_timezones = [sample.timeZone for sample in result]

        expected_timezones = 12 * [dst_info.standard_timezone]

        # The last two samples shouldn't have a timezone set as they're more
        # than an hour after the transition point
        # noinspection PyTypeChecker
        expected_timezones = expected_timezones + [None, None]

        self.assertListEqual(result_timezones, expected_timezones)

    def test_timezone_set_for_hour_after_dst_off_half_way_through(self):
        """
        For an hour after the daylight savings off transition point the DST
        Switcher should set the timezone on all samples to the standard
        timezone. This ensures that there is no confusion at the database layer
        between, for example, the timestamps 5-APR-2015 02:30 NZDT and
        5-APR-2015 02:30 NZST.

        This test starts the logger half way through the hour and issues
        the suppress dst off command. It should result in samples getting the
        standard time zone
        """

        dst_info = FakeDstInfo()

        sample_interval = 5
        sample_interval_seconds = sample_interval*60

        # Start TS is 20 minutes after the DST end point. DST Switcher won't
        # be able to detect if we're before the transition or after
        start_ts = dst_info.dst_end - timedelta(0, 4*sample_interval_seconds)
        prev_ts = start_ts - timedelta(0, sample_interval_seconds)

        switcher = DstSwitcher(dst_info, sample_interval, prev_ts)

        # So we will tell the DST Switcher we're after the transition:
        switcher.suppress_dst_off()

        samples = self._make_samples(start_ts, sample_interval, None, 10)

        result = [switcher.process_sample(sample) for sample in samples]

        result_timezones = [sample.timeZone for sample in result]

        expected_timezones = 4 * [dst_info.standard_timezone]

        # The last two samples shouldn't have a timezone set as they're more
        # than an hour after the transition point
        # noinspection PyTypeChecker
        expected_timezones = expected_timezones + 6 * [None]

        self.assertListEqual(result_timezones, expected_timezones)

    def test_timezone_set_for_hour_before_dst_off_half_way_through(self):
        """
        For an hour after the daylight savings off transition point the DST
        Switcher should set the timezone on all samples to the standard
        timezone. This ensures that there is no confusion at the database layer
        between, for example, the timestamps 5-APR-2015 02:30 NZDT and
        5-APR-2015 02:30 NZST.

        This test starts the logger half way through the hour and does not issue
        the suppress dst off command. It should result in samples getting the
        DST time zone
        """

        dst_info = FakeDstInfo()

        sample_interval = 5
        sample_interval_seconds = sample_interval*60

        # Start TS is 20 minutes after the DST end point. DST Switcher won't
        # be able to detect if we're before the transition or after. It should
        # assume we're before.
        start_ts = dst_info.dst_end - timedelta(0, 4*sample_interval_seconds)
        prev_ts = start_ts - timedelta(0, sample_interval_seconds)

        switcher = DstSwitcher(dst_info, sample_interval, prev_ts)

        samples = self._make_samples(start_ts, sample_interval, None, 8)

        result = [switcher.process_sample(sample) for sample in samples]

        result_timezones = [sample.timeZone for sample in result]

        expected_timezones = 4 * [dst_info.dst_timezone] + \
            4 * [dst_info.standard_timezone]

        tsses = [(x.timeStamp.hour, x.timeStamp.minute) for x in samples]

        print("TC: " + str(tsses))
        print("EX: " + str(expected_timezones))
        print("RE: " + str(result_timezones))

        self.assertListEqual(result_timezones, expected_timezones)


class DstInfoTests(unittest.TestCase):

    def test_dst_timezone(self):
        info = DstInfo("Pacific/Auckland")

        # NZDT is GMT+13 (or 780 minutes)
        self.assertEqual(info.dst_timezone, 780)

    def test_standard_timezone(self):
        info = DstInfo("Pacific/Auckland")

        # NZST is GMT+12 (or 720 minutes)
        self.assertEqual(info.standard_timezone, 720)

    def test_nearest_dst_end(self):

        def _static_time():
            return datetime(2015, 2, 6, 18, 36, 0)

        # Nearest DST end transition to the above time for Pacific/Auckland is:
        #   5-APR-2015 03:00

        info = DstInfo("Pacific/Auckland", _static_time)

        self.assertEqual(info.dst_end, datetime(2015, 4, 5, 3, 0, 0))

    def test_nearest_dst_start(self):

        def _static_time():
            return datetime(2015, 2, 6, 18, 36, 0)

        # Nearest DST start transition to the above time for Pacific/Auckland
        # is:
        #   28-SEP-2014 02:00

        info = DstInfo("Pacific/Auckland", _static_time)

        self.assertEqual(info.dst_start, datetime(2014, 9, 28, 2, 0, 0))

    def test_nearest_dst_end_future(self):
        # Try another date off in the future to double check that it really
        # is calculating the DST

        def _static_time():
            return datetime(2019, 3, 1, 0, 0, 0)

        # Nearest DST end transition to the above time for Pacific/Auckland is:
        #   7-APR-2019

        info = DstInfo("Pacific/Auckland", _static_time)

        self.assertEqual(info.dst_end, datetime(2019, 4, 7, 3, 0, 0))

    def test_nearest_dst_start_future(self):
        # Try another date off in the future to double check that it really
        # is calculating the DST

        def _static_time():
            return datetime(2019, 8, 1, 0, 0, 0)

        # Nearest DST start transition to the above time for Pacific/Auckland
        # is:
        #   29-SEP-2019 02:00

        info = DstInfo("Pacific/Auckland", _static_time)

        self.assertEqual(info.dst_start, datetime(2019, 9, 29, 2, 0, 0))

    def test_nearest_dst_end_edge(self):
        """
        The DstInfo class should return the nearest transition point of the
        requested type. Nearest is defined as being within ~182.621 days (half
        a year). On either side of this point we should get different answers
        for the dst end transition time.
        """

        edge_a = datetime(2014, 10, 5, 3, 0, 0)
        answer_a = datetime(2014, 4, 6, 3, 0, 0)

        edge_b = datetime(2014, 10, 5, 3, 0, 1)
        answer_b = datetime(2015, 4, 5, 3, 0, 0)

        def _time_a():
            return edge_a

        def _time_b():
            return edge_b

        info_a = DstInfo("Pacific/Auckland", _time_a)
        info_b = DstInfo("Pacific/Auckland", _time_b)

        self.assertEqual(info_a.dst_end, answer_a)
        self.assertEqual(info_b.dst_end, answer_b)

    def test_nearest_dst_start_edge(self):
        """
        The DstInfo class should return the nearest transition point of the
        requested type. Nearest is defined as being within ~182.621 days (half
        a year). On either side of this point we should get different answers
        for the dst start transition time.
        """

        edge_a = datetime(2015, 3, 29, 2, 0, 0)
        answer_a = datetime(2014, 9, 28, 2, 0, 0)

        edge_b = datetime(2015, 3, 29, 2, 0, 1)
        answer_b = datetime(2015, 9, 27, 2, 0, 0)

        def _time_a():
            return edge_a

        def _time_b():
            return edge_b

        info_a = DstInfo("Pacific/Auckland", _time_a)
        info_b = DstInfo("Pacific/Auckland", _time_b)

        self.assertEqual(info_a.dst_start, answer_a)
        self.assertEqual(info_b.dst_start, answer_b)



