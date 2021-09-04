"""
Unit tests for data subscriptions
"""
import datetime
import unittest

from server.subscriptions import StationSubscriptionManager


class TestSubscriber(object):
    def __init__(self):
        self.samples = []
        self.lives = []

    def sample_data(self, data):
        self.samples.append(data)

    def live_data(self, data):
        self.lives.append(data)


class StationSubscriptionManagerTestCase(unittest.TestCase):

    def test_image_subscriber_receives_only_images(self):
        ssm = StationSubscriptionManager('x')
        sub = TestSubscriber()

        ssm.subscribe(sub, False, False, True)

        self.assertEqual(0, len(sub.samples))
        self.assertEqual(0, len(sub.lives))

        ssm.deliver_image("Test Image")

        self.assertEqual(0, len(sub.samples))
        self.assertEqual(1, len(sub.lives))
        self.assertEqual("Test Image", sub.lives[0])

    def test_sample_subscriber_receives_only_samples(self):
        ssm = StationSubscriptionManager('x')
        sub = TestSubscriber()

        ssm.subscribe(sub, False, True, False)

        self.assertEqual(0, len(sub.samples))
        self.assertEqual(0, len(sub.lives))

        dt = datetime.datetime.now()

        sample = (dt, "Test Sample")
        expected = "s,Test Sample"

        ssm.deliver_sample([sample, ], 1)

        self.assertEqual(1, len(sub.samples))
        self.assertEqual(0, len(sub.lives))
        self.assertEqual(expected, sub.samples[0])

    def test_sample_subscriber_receives_only_samples_of_requested_format(self):
        ssm = StationSubscriptionManager('x')
        sub = TestSubscriber()

        ssm.subscribe(sub, False, True, False, sample_format=2)

        self.assertEqual(0, len(sub.samples))
        self.assertEqual(0, len(sub.lives))

        dt = datetime.datetime.now()
        dt2 = dt + datetime.timedelta(hours=1)

        ssm.deliver_sample([(dt, "Test Sample 1")], 1)

        self.assertEqual(0, len(sub.samples))
        self.assertEqual(0, len(sub.lives))

        ssm.deliver_sample([(dt2, "Test Sample 2")], 2)

        self.assertEqual(1, len(sub.samples))
        self.assertEqual(0, len(sub.lives))
        self.assertEqual("s,Test Sample 2", sub.samples[0])

    def test_live_subscriber_receives_only_lives(self):
        ssm = StationSubscriptionManager('x')
        sub = TestSubscriber()

        ssm.subscribe(sub, True, False, False)

        self.assertEqual(0, len(sub.samples))
        self.assertEqual(0, len(sub.lives))

        ssm.deliver_live("Test Live", 1)

        self.assertEqual(0, len(sub.samples))
        self.assertEqual(1, len(sub.lives))
        self.assertEqual("Test Live", sub.lives[0])

    def test_live_subscriber_receives_only_lives_of_requested_format(self):
        ssm = StationSubscriptionManager('x')
        sub = TestSubscriber()

        ssm.subscribe(sub, True, False, False, live_format=2)

        self.assertEqual(0, len(sub.samples))
        self.assertEqual(0, len(sub.lives))

        ssm.deliver_live("Test Live 1", 1)

        self.assertEqual(0, len(sub.samples))
        self.assertEqual(0, len(sub.lives))

        ssm.deliver_live("Test Live 2", 2)

        self.assertEqual(0, len(sub.samples))
        self.assertEqual(1, len(sub.lives))
        self.assertEqual("Test Live 2", sub.lives[0])

    def test_enabled_live_record_formats(self):
        ssm = StationSubscriptionManager('x')
        sub1 = TestSubscriber()
        sub2 = TestSubscriber()

        self.assertEqual([], ssm.enabled_live_record_formats())
        ssm.subscribe(sub1, True, False, False, live_format=1)
        self.assertEqual([1, ], ssm.enabled_live_record_formats())
        ssm.subscribe(sub2, True, False, False, live_format=2)
        self.assertEqual([1, 2], ssm.enabled_live_record_formats())

    def test_enabled_sample_record_formats(self):
        ssm = StationSubscriptionManager('x')
        sub1 = TestSubscriber()
        sub2 = TestSubscriber()

        self.assertEqual(set(), ssm.enabled_sample_record_formats())
        ssm.subscribe(sub1, False, True, False, sample_format=1)
        self.assertEqual({1}, ssm.enabled_sample_record_formats())
        ssm.subscribe(sub2, False, True, False, sample_format=2)
        self.assertEqual({1, 2}, ssm.enabled_sample_record_formats())

    def test_only_live_formats_with_subscribers_are_enabled(self):
        ssm = StationSubscriptionManager('x')
        sub1 = TestSubscriber()
        sub2 = TestSubscriber()

        self.assertEqual([], ssm.enabled_live_record_formats())
        ssm.subscribe(sub1, True, False, False, live_format=1)
        self.assertEqual([1, ], ssm.enabled_live_record_formats())
        ssm.subscribe(sub2, True, False, False, live_format=2)
        self.assertEqual([1, 2], ssm.enabled_live_record_formats())
        
        ssm.unsubscribe_all(sub2)
        self.assertEqual([1, ], ssm.enabled_live_record_formats())
        ssm.unsubscribe_all(sub1)
        self.assertEqual([], ssm.enabled_live_record_formats())

    def test_multi_subscribe(self):
        ssm = StationSubscriptionManager('x')
        sub = TestSubscriber()

        ssm.subscribe(sub, True, True, True)

        self.assertEqual(0, len(sub.samples))
        self.assertEqual(0, len(sub.lives))

        dt = datetime.datetime.now()

        sample = (dt, "Test Sample")
        expected = "s,Test Sample"

        ssm.deliver_sample([sample, ], 1)
        ssm.deliver_image("image")
        ssm.deliver_live("live", 1)

        self.assertEqual(1, len(sub.samples))
        self.assertEqual(2, len(sub.lives))
        self.assertEqual(expected, sub.samples[0])
        self.assertEqual(["image", "live"], sub.lives)

    def test_unsubscribe_live(self):
        ssm = StationSubscriptionManager('x')
        sub = TestSubscriber()

        ssm.subscribe(sub, True, False, False)

        self.assertEqual(0, len(sub.samples))
        self.assertEqual(0, len(sub.lives))

        ssm.deliver_live("Test Live", 1)

        self.assertEqual(0, len(sub.samples))
        self.assertEqual(1, len(sub.lives))
        self.assertEqual("Test Live", sub.lives[0])
        
        ssm.remove_live_subscription(sub)

        ssm.deliver_live("Test Live 2", 1)
        
        self.assertEqual(1, len(sub.lives))
        self.assertEqual("Test Live", sub.lives[0])

    def test_unsubscribe_ordered_sample(self):
        ssm = StationSubscriptionManager('x')
        sub = TestSubscriber()

        ssm.subscribe(sub, False, True, False)

        self.assertEqual(0, len(sub.samples))
        self.assertEqual(0, len(sub.lives))

        dt = datetime.datetime.now()

        sample = (dt, "Test Sample")
        expected = "s,Test Sample"

        ssm.deliver_sample([sample, ], 1)

        self.assertEqual(1, len(sub.samples))
        self.assertEqual(0, len(sub.lives))
        self.assertEqual(expected, sub.samples[0])

        ssm.remove_sample_subscription(sub)

        ssm.deliver_sample([sample, ], 1)

        self.assertEqual(1, len(sub.samples))
        self.assertEqual(0, len(sub.lives))
        self.assertEqual(expected, sub.samples[0])

    def test_unsubscribe_unordered_sample(self):
        ssm = StationSubscriptionManager('x')
        sub = TestSubscriber()

        ssm.subscribe(sub, False, True, False, samples_in_any_order=True)

        self.assertEqual(0, len(sub.samples))
        self.assertEqual(0, len(sub.lives))

        dt = datetime.datetime.now()

        sample = (dt, "Test Sample")
        expected = "s,Test Sample"

        ssm.deliver_sample([sample, ], 1)

        self.assertEqual(1, len(sub.samples))
        self.assertEqual(0, len(sub.lives))
        self.assertEqual(expected, sub.samples[0])

        ssm.remove_sample_subscription(sub)

        ssm.deliver_sample([sample, ], 1)

        self.assertEqual(1, len(sub.samples))
        self.assertEqual(0, len(sub.lives))
        self.assertEqual(expected, sub.samples[0])

    def test_unsubscribe_image(self):
        ssm = StationSubscriptionManager('x')
        sub = TestSubscriber()

        ssm.subscribe(sub, False, False, True)

        self.assertEqual(0, len(sub.samples))
        self.assertEqual(0, len(sub.lives))

        ssm.deliver_image("Test Image")

        self.assertEqual(0, len(sub.samples))
        self.assertEqual(1, len(sub.lives))
        self.assertEqual("Test Image", sub.lives[0])

        ssm.remove_image_subscription(sub)

        ssm.deliver_image("Test Image")

        self.assertEqual(0, len(sub.samples))
        self.assertEqual(1, len(sub.lives))
        self.assertEqual("Test Image", sub.lives[0])

    def test_unsubscribe_all(self):
        ssm = StationSubscriptionManager('x')
        sub = TestSubscriber()

        ssm.subscribe(sub, True, True, True)

        self.assertEqual(0, len(sub.samples))
        self.assertEqual(0, len(sub.lives))

        dt = datetime.datetime.now()

        sample = (dt, "Test Sample")
        expected = "s,Test Sample"

        ssm.deliver_sample([sample, ], 1)
        ssm.deliver_image("image")
        ssm.deliver_live("live", 1)

        self.assertEqual(1, len(sub.samples))
        self.assertEqual(2, len(sub.lives))
        self.assertEqual(expected, sub.samples[0])
        self.assertEqual(["image", "live"], sub.lives)

        ssm.unsubscribe_all(sub)

        ssm.deliver_sample([sample, ], 1)
        ssm.deliver_image("image")
        ssm.deliver_live("live", 1)

        self.assertEqual(1, len(sub.samples))
        self.assertEqual(2, len(sub.lives))
        self.assertEqual(expected, sub.samples[0])
        self.assertEqual(["image", "live"], sub.lives)

    def test_unsubscribe_live_of_type(self):
        ssm = StationSubscriptionManager('x')
        sub = TestSubscriber()

        ssm.subscribe(sub, True, False, False, live_format=1)
        ssm.subscribe(sub, True, False, False, live_format=2)

        self.assertEqual(0, len(sub.samples))
        self.assertEqual(0, len(sub.lives))

        ssm.deliver_live("Test Live 1", 1)
        ssm.deliver_live("Test Live 2", 2)

        self.assertEqual(0, len(sub.samples))
        self.assertEqual(2, len(sub.lives))
        self.assertEqual(["Test Live 1", "Test Live 2"], sub.lives)

        ssm.remove_live_subscription(sub, 2)

        ssm.deliver_live("Test Live 1", 1)
        ssm.deliver_live("Test Live 2", 2)

        self.assertEqual(0, len(sub.samples))
        self.assertEqual(3, len(sub.lives))
        self.assertEqual(["Test Live 1", "Test Live 2", "Test Live 1"],
                         sub.lives)

    def test_ordered_sample_delivery(self):
        ssm = StationSubscriptionManager('x')
        sub = TestSubscriber()

        ssm.subscribe(sub, False, True, False)

        self.assertEqual(0, len(sub.samples))
        self.assertEqual(0, len(sub.lives))

        dt = datetime.datetime.now()
        dt2 = dt + datetime.timedelta(hours=1)
        dt3 = dt2 + datetime.timedelta(hours=1)
        dt4 = dt3 + datetime.timedelta(hours=1)

        samples = [
            (dt, "Test 1"),
            (dt3, "Test 3"),
            (dt2, "Test 2"),
            (dt4, "Test 4")
        ]

        expected = [
            "s,Test 1",
            "s,Test 3",
            "s,Test 4"
        ]

        ssm.deliver_sample(samples, 1)

        self.assertEqual(3, len(sub.samples))
        self.assertEqual(0, len(sub.lives))
        self.assertEqual(expected, sub.samples)

    def test_unordered_sample_delivery(self):
        ssm = StationSubscriptionManager('x')
        sub = TestSubscriber()

        ssm.subscribe(sub, False, True, False, samples_in_any_order=True)

        self.assertEqual(0, len(sub.samples))
        self.assertEqual(0, len(sub.lives))

        dt = datetime.datetime.now()
        dt2 = dt + datetime.timedelta(hours=1)
        dt3 = dt2 + datetime.timedelta(hours=1)
        dt4 = dt3 + datetime.timedelta(hours=1)

        samples = [
            (dt, "Test 1"),
            (dt3, "Test 3"),
            (dt2, "Test 2"),
            (dt4, "Test 4")
        ]

        expected = [
            "s,Test 1",
            "s,Test 3",
            "s,Test 2",
            "s,Test 4"
        ]

        ssm.deliver_sample(samples, 1)

        self.assertEqual(4, len(sub.samples))
        self.assertEqual(0, len(sub.lives))
        self.assertEqual(expected, sub.samples)

    def test_instant_live_delivery(self):
        ssm = StationSubscriptionManager('x')
        sub = TestSubscriber()
        sub2 = TestSubscriber()

        ssm.subscribe(sub, True, False, False)

        self.assertEqual(0, len(sub.samples))
        self.assertEqual(0, len(sub.lives))

        ssm.deliver_live("Test Live", 1)

        self.assertEqual(0, len(sub.samples))
        self.assertEqual(1, len(sub.lives))
        self.assertEqual("Test Live", sub.lives[0])

        self.assertEqual(0, len(sub2.samples))
        self.assertEqual(0, len(sub2.lives))

        ssm.subscribe(sub2, True, False, False)

        self.assertEqual(0, len(sub2.samples))
        self.assertEqual(1, len(sub2.lives))
        self.assertEqual("Test Live", sub2.lives[0])
