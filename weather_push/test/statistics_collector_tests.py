import unittest
from datetime import datetime

from zxw_push.common.statistics_collector import StatisticsCollector, MultiPeriodStatisticsCollector


class StatisticsCollectorTests(unittest.TestCase):
    def test_logging_a_live_record_increases_count_of_live_records(self):
        c = StatisticsCollector()

        before = c.get_live_statistics()
        c.log_live_record(0, 0, "skip")
        after = c.get_live_statistics()

        self.assertEqual(
            after["count"],
            before["count"]+1,
            "Live count should be incremented by one"
        )

    def test_logging_a_live_record_increases_count_for_algorithm(self):
        c = StatisticsCollector()

        algorithm = "skip"

        c.log_live_record(0, 0, algorithm)
        after = c.get_live_statistics()

        self.assertEqual(
            after["algorithm"][algorithm]["count"],
            1,
            "Live count for algorithm should be incremented by one"
        )

    def test_logging_a_live_Record_increases_total_unencoded_size(self):
        c = StatisticsCollector()

        algorithm = "skip"

        c.log_live_record(0, 5, algorithm)
        after = c.get_live_statistics()

        self.assertEqual(
            after["algorithm"][algorithm]["total_unencoded_size"],
            5,
            "Total unencoded size should be incremented by 5"
        )

    def test_logging_a_live_Record_increases_total_encoded_size(self):
        c = StatisticsCollector()

        algorithm = "skip"

        c.log_live_record(3, 5, algorithm)
        after = c.get_live_statistics()

        self.assertEqual(
            after["algorithm"][algorithm]["total_encoded_size"],
            3,
            "Total encoded size should be incremented by 5"
        )

    def test_logging_a_sample_record_increases_count_of_sample_records(self):
        c = StatisticsCollector()

        before = c.get_sample_statistics()
        c.log_sample_record(0, 0, "none")
        after = c.get_sample_statistics()

        self.assertEqual(
            after["count"],
            before["count"]+1,
            "Sample count should be incremented by one"
        )

    def test_logging_a_sample_record_increases_count_for_algorithm(self):
        c = StatisticsCollector()

        algorithm = "none"

        c.log_sample_record(0, 0, algorithm)
        after = c.get_sample_statistics()

        self.assertEqual(
            after["algorithm"][algorithm]["count"],
            1,
            "Sample count for algorithm should be incremented by one"
        )

    def test_logging_a_sample_Record_increases_total_unencoded_size(self):
        c = StatisticsCollector()

        algorithm = "none"

        c.log_sample_record(0, 5, algorithm)
        after = c.get_sample_statistics()

        self.assertEqual(
            after["algorithm"][algorithm]["total_unencoded_size"],
            5,
            "Total unencoded size should be incremented by 5"
        )

    def test_logging_a_sample_Record_increases_total_encoded_size(self):
        c = StatisticsCollector()

        algorithm = "none"

        c.log_sample_record(3, 5, algorithm)
        after = c.get_sample_statistics()

        self.assertEqual(
            after["algorithm"][algorithm]["total_encoded_size"],
            3,
            "Total encoded size should be incremented by 5"
        )

    def test_logging_packet_increments_packet_count(self):
        c = StatisticsCollector()

        before = c.get_packet_statistics()
        c.log_packet_transmission("test", 50)
        after = c.get_packet_statistics()

        self.assertEqual(
            after["count"],
            before["count"]+1,
            "Packet count should be incremented by one"
        )

    def test_logging_packet_type_increments_count_for_that_type(self):
        c = StatisticsCollector()

        c.log_packet_transmission("test", 50)
        after = c.get_packet_statistics()

        self.assertEqual(
            after["type"]["test"]["count"],
            1,
            "Packet count for type 'test' should be incremented by one"
        )

    def test_logging_packet_type_increments_total_size_for_that_type(self):
        c = StatisticsCollector()

        c.log_packet_transmission("test", 50)
        after = c.get_packet_statistics()

        self.assertEqual(
            after["type"]["test"]["total_size"],
            50,
            "Packet total size for type 'test' should be incremented by 50"
        )

    def test_reset_statistics(self):
        c = StatisticsCollector()

        # Make some statistics
        c.log_sample_record(3, 5, "none")
        c.log_live_record(3, 5, "skip")
        c.log_packet_transmission("test", 50)

        # Clear them
        c.reset_statistics()

        # Check they're cleared
        self.assertDictEqual(
            c.get_live_statistics(),
            {
                "count": 0,
                "algorithm": dict()
            },
            "Live statistics should be cleared"
        )

        self.assertDictEqual(
            c.get_sample_statistics(),
            {
                "count": 0,
                "algorithm": dict()
            },
            "Sample statistics should be cleared"
        )

        self.assertDictEqual(
            c.get_packet_statistics(),
            {
                "count": 0,
                "type": dict()
            },
            "Packet statistics should be cleared"
        )


class MultiPeriodStatisticsCollectorTests(unittest.TestCase):
    """
    This primarily tests that statistics for the various time periods are reset appropriately.
    """
    def test_logging_a_live_record_increments_all_appropriate_statistics(self):

        t = datetime.now()

        def time_src():
            return t

        c = MultiPeriodStatisticsCollector(time_src)
        c.log_live_record(3, 5, "skip")

        stats = c.get_live_statistics()

        expected_stats = {
            "count": 1,
            "algorithm": {
                "skip": {
                    "count": 1,
                    "total_unencoded_size": 5,
                    "total_encoded_size": 3
                }
            }
        }

        self.assertDictEqual(
            stats,
            {
                "today": expected_stats,
                "week": expected_stats,
                "month": expected_stats,
                "all_time": expected_stats
            }
        )

    def test_logging_a_sample_record_increments_all_appropriate_statistics(self):
        t = datetime.now()

        def time_src():
            return t

        c = MultiPeriodStatisticsCollector(time_src)
        c.log_sample_record(3, 5, "none")

        stats = c.get_sample_statistics()

        expected_stats = {
            "count": 1,
            "algorithm": {
                "none": {
                    "count": 1,
                    "total_unencoded_size": 5,
                    "total_encoded_size": 3
                }
            }
        }

        self.assertDictEqual(
            stats,
            {
                "today": expected_stats,
                "week": expected_stats,
                "month": expected_stats,
                "all_time": expected_stats
            }
        )

    def test_logging_a_packet_increments_all_appropriate_statistics(self):
        t = datetime.now()

        def time_src():
            return t

        c = MultiPeriodStatisticsCollector(time_src)
        c.log_packet_transmission("test", 50)

        stats = c.get_packet_statistics()

        expected_stats = {
            "count": 1,
            "type": {
                "test": {
                    "count": 1,
                    "total_size": 50,
                }
            }
        }

        self.assertDictEqual(
            stats,
            {
                "today": expected_stats,
                "week": expected_stats,
                "month": expected_stats,
                "all_time": expected_stats
            }
        )
        #c.log_packet_transmission("test", 50)

    def test_live_day_reset(self):
        t = datetime(year=2020, month=6, day=1, hour=23, minute=55)

        def time_src():
            return t

        c = MultiPeriodStatisticsCollector(time_src)
        c.log_live_record(3, 5, "skip")

        # Check statistics have been logged...
        stats = c.get_live_statistics()

        expected_stats = {
            "count": 1,
            "algorithm": {
                "skip": {
                    "count": 1,
                    "total_unencoded_size": 5,
                    "total_encoded_size": 3
                }
            }
        }

        self.assertDictEqual(
            stats,
            {
                "today": expected_stats,
                "week": expected_stats,
                "month": expected_stats,
                "all_time": expected_stats
            },
            "Statistics should be logged for current date (and others)"
        )

        # Move on to the next day
        t = datetime(year=2020, month=6, day=2, hour=0, minute=0)

        # Log some statistics. This should cause the day statistics to be cleared before
        # this live record is logged
        c.log_live_record(2, 5, "live-diff")

        stats = c.get_live_statistics()

        expected_stats = {
            "count": 2,
            "algorithm": {
                "skip": {
                    "count": 1,
                    "total_unencoded_size": 5,
                    "total_encoded_size": 3
                },
                "live-diff": {
                    "count": 1,
                    "total_unencoded_size": 5,
                    "total_encoded_size": 2
                }
            }
        }

        self.assertDictEqual(
            stats,
            {
                "today": {
                    "count": 1,
                    "algorithm": {
                        "live-diff": {
                            "count": 1,
                            "total_unencoded_size": 5,
                            "total_encoded_size": 2
                        }
                    }
                },
                "week": expected_stats,
                "month": expected_stats,
                "all_time": expected_stats
            },
            "Statistics should have been cleared for current day without affecting others"
        )

    def test_live_two_day_resets(self):
        t = datetime(year=2020, month=6, day=1, hour=23, minute=55)

        def time_src():
            return t

        c = MultiPeriodStatisticsCollector(time_src)
        c.log_live_record(3, 5, "skip")

        # Check statistics have been logged...
        stats = c.get_live_statistics()

        expected_stats = {
            "count": 1,
            "algorithm": {
                "skip": {
                    "count": 1,
                    "total_unencoded_size": 5,
                    "total_encoded_size": 3
                }
            }
        }

        self.assertDictEqual(
            stats,
            {
                "today": expected_stats,
                "week": expected_stats,
                "month": expected_stats,
                "all_time": expected_stats
            },
            "Statistics should be logged for current date (and others)"
        )

        # Move on to the next day
        t = datetime(year=2020, month=6, day=2, hour=0, minute=0)

        # Log some statistics. This should cause the day statistics to be cleared before
        # this live record is logged
        c.log_live_record(2, 5, "live-diff")

        stats = c.get_live_statistics()

        expected_stats = {
            "count": 2,
            "algorithm": {
                "skip": {
                    "count": 1,
                    "total_unencoded_size": 5,
                    "total_encoded_size": 3
                },
                "live-diff": {
                    "count": 1,
                    "total_unencoded_size": 5,
                    "total_encoded_size": 2
                }
            }
        }

        self.assertDictEqual(
            stats,
            {
                "today": {
                    "count": 1,
                    "algorithm": {
                        "live-diff": {
                            "count": 1,
                            "total_unencoded_size": 5,
                            "total_encoded_size": 2
                        }
                    }
                },
                "week": expected_stats,
                "month": expected_stats,
                "all_time": expected_stats
            },
            "Statistics should have been cleared for current day without affecting others"
        )

        # Log some more data
        t = datetime(year=2020, month=6, day=2, hour=0, minute=5)

        # Log some statistics. This should cause the day statistics to be cleared again
        # before this live record is logged
        c.log_live_record(1, 5, "skip")

        stats = c.get_live_statistics()

        expected_stats = {
            "count": 3,
            "algorithm": {
                "skip": {
                    "count": 2,
                    "total_unencoded_size": 10,
                    "total_encoded_size": 4
                },
                "live-diff": {
                    "count": 1,
                    "total_unencoded_size": 5,
                    "total_encoded_size": 2
                }
            }
        }

        self.assertDictEqual(
            stats,
            {
                "today": {
                    "count": 2,
                    "algorithm": {
                        "live-diff": {
                            "count": 1,
                            "total_unencoded_size": 5,
                            "total_encoded_size": 2
                        },
                        "skip": {
                            "count": 1,
                            "total_unencoded_size": 5,
                            "total_encoded_size": 1
                        }
                    }
                },
                "week": expected_stats,
                "month": expected_stats,
                "all_time": expected_stats
            },
            "Statistics should not have been reset"
        )

        # Move on to another day
        t = datetime(year=2020, month=6, day=3, hour=0, minute=0)

        # Log some statistics. This should cause the day statistics to be cleared again
        # before this live record is logged
        c.log_live_record(2, 5, "live-diff")

        stats = c.get_live_statistics()

        expected_stats = {
            "count": 4,
            "algorithm": {
                "skip": {
                    "count": 2,
                    "total_unencoded_size": 10,
                    "total_encoded_size": 4
                },
                "live-diff": {
                    "count": 2,
                    "total_unencoded_size": 10,
                    "total_encoded_size": 4
                }
            }
        }

        self.assertDictEqual(
            stats,
            {
                "today": {
                    "count": 1,
                    "algorithm": {
                        "live-diff": {
                            "count": 1,
                            "total_unencoded_size": 5,
                            "total_encoded_size": 2
                        }
                    }
                },
                "week": expected_stats,
                "month": expected_stats,
                "all_time": expected_stats
            },
            "Statistics should have been cleared again for current day without affecting others"
        )

    def test_live_month_reset(self):
        t = datetime(year=2020, month=6, day=30, hour=23, minute=55)

        def time_src():
            return t

        c = MultiPeriodStatisticsCollector(time_src)
        c.log_live_record(3, 5, "skip")

        # Check statistics have been logged...
        stats = c.get_live_statistics()

        expected_stats = {
            "count": 1,
            "algorithm": {
                "skip": {
                    "count": 1,
                    "total_unencoded_size": 5,
                    "total_encoded_size": 3
                }
            }
        }

        self.assertDictEqual(
            stats,
            {
                "today": expected_stats,
                "week": expected_stats,
                "month": expected_stats,
                "all_time": expected_stats
            },
            "Statistics should be logged for current date (and others)"
        )

        # Move on to the next day (and next month)
        t = datetime(year=2020, month=7, day=1, hour=0, minute=0)

        # Log some statistics. This should cause the day & month statistics to be cleared before
        # this live record is logged
        c.log_live_record(2, 5, "live-diff")

        stats = c.get_live_statistics()

        expected_reset_stats = {
                    "count": 1,
                    "algorithm": {
                        "live-diff": {
                            "count": 1,
                            "total_unencoded_size": 5,
                            "total_encoded_size": 2
                        }
                    }
                }
        expected_unchanged_stats = {
            "count": 2,
            "algorithm": {
                "skip": {
                    "count": 1,
                    "total_unencoded_size": 5,
                    "total_encoded_size": 3
                },
                "live-diff": {
                    "count": 1,
                    "total_unencoded_size": 5,
                    "total_encoded_size": 2
                }
            }
        }

        self.assertDictEqual(
            stats,
            {
                "today": expected_reset_stats,
                "week": expected_unchanged_stats,
                "month": expected_reset_stats,
                "all_time": expected_unchanged_stats
            },
            "Statistics should have been cleared for current day without affecting others"
        )

        # Log some more data. This shouldn't reset anything
        t = datetime(year=2020, month=7, day=1, hour=0, minute=5)
        c.log_live_record(2, 5, "live-diff")

        stats = c.get_live_statistics()

        expected_reset_stats = {
            "count": 2,
            "algorithm": {
                "live-diff": {
                    "count": 2,
                    "total_unencoded_size": 10,
                    "total_encoded_size": 4
                }
            }
        }
        expected_unchanged_stats = {
            "count": 3,
            "algorithm": {
                "skip": {
                    "count": 1,
                    "total_unencoded_size": 5,
                    "total_encoded_size": 3
                },
                "live-diff": {
                    "count": 2,
                    "total_unencoded_size": 10,
                    "total_encoded_size": 4
                }
            }
        }

        self.assertDictEqual(
            stats,
            {
                "today": expected_reset_stats,
                "week": expected_unchanged_stats,
                "month": expected_reset_stats,
                "all_time": expected_unchanged_stats
            },
            "Statistics should not have been reset"
        )

        # Move on to the next month
        t = datetime(year=2020, month=8, day=1, hour=0, minute=0)

        # Log some statistics. This should cause the day, week & month statistics to be cleared
        # before this live record is logged
        c.log_live_record(2, 5, "live-diff")

        stats = c.get_live_statistics()

        expected_reset_stats = {
            "count": 1,
            "algorithm": {
                "live-diff": {
                    "count": 1,
                    "total_unencoded_size": 5,
                    "total_encoded_size": 2
                }
            }
        }
        expected_unchanged_stats = {
            "count": 4,
            "algorithm": {
                "skip": {
                    "count": 1,
                    "total_unencoded_size": 5,
                    "total_encoded_size": 3
                },
                "live-diff": {
                    "count": 3,
                    "total_unencoded_size": 15,
                    "total_encoded_size": 6
                }
            }
        }

        self.assertDictEqual(
            stats,
            {
                "today": expected_reset_stats,
                "week": expected_reset_stats,
                "month": expected_reset_stats,
                "all_time": expected_unchanged_stats
            },
            "Statistics should have been reset for day and month"
        )

    def test_live_week_reset(self):
        t = datetime(year=2020, month=5, day=31, hour=23, minute=55)

        def time_src():
            return t

        c = MultiPeriodStatisticsCollector(time_src)
        c.log_live_record(3, 5, "skip")

        # Check statistics have been logged...
        stats = c.get_live_statistics()

        expected_stats = {
            "count": 1,
            "algorithm": {
                "skip": {
                    "count": 1,
                    "total_unencoded_size": 5,
                    "total_encoded_size": 3
                }
            }
        }

        self.assertDictEqual(
            stats,
            {
                "today": expected_stats,
                "week": expected_stats,
                "month": expected_stats,
                "all_time": expected_stats
            },
            "Statistics should be logged for current date (and others)"
        )

        # Move on to the next day
        t = datetime(year=2020, month=6, day=1, hour=0, minute=0)

        # Log some statistics. This should cause the day, week and month statistics to be cleared
        # before this live record is logged
        c.log_live_record(2, 5, "live-diff")

        stats = c.get_live_statistics()

        expected_unchanged_stats = {
            "count": 2,
            "algorithm": {
                "skip": {
                    "count": 1,
                    "total_unencoded_size": 5,
                    "total_encoded_size": 3
                },
                "live-diff": {
                    "count": 1,
                    "total_unencoded_size": 5,
                    "total_encoded_size": 2
                }
            }
        }

        expected_reset_stats = {
                    "count": 1,
                    "algorithm": {
                        "live-diff": {
                            "count": 1,
                            "total_unencoded_size": 5,
                            "total_encoded_size": 2
                        }
                    }
                }

        self.assertDictEqual(
            stats,
            {
                "today": expected_reset_stats,
                "week": expected_reset_stats,
                "month": expected_reset_stats,
                "all_time": expected_unchanged_stats
            },
            "Statistics should have been cleared for current day and week without affecting others"
        )

        t = datetime(year=2020, month=6, day=1, hour=0, minute=5)

        # Log some statistics. This shouldn't cause anything to be reset
        c.log_live_record(2, 5, "live-diff")

        stats = c.get_live_statistics()

        expected_unchanged_stats = {
            "count": 3,
            "algorithm": {
                "skip": {
                    "count": 1,
                    "total_unencoded_size": 5,
                    "total_encoded_size": 3
                },
                "live-diff": {
                    "count": 2,
                    "total_unencoded_size": 10,
                    "total_encoded_size": 4
                }
            }
        }

        expected_reset_stats = {
            "count": 2,
            "algorithm": {
                "live-diff": {
                    "count": 2,
                    "total_unencoded_size": 10,
                    "total_encoded_size": 4
                }
            }
        }

        self.assertDictEqual(
            stats,
            {
                "today": expected_reset_stats,
                "week": expected_reset_stats,
                "month": expected_reset_stats,
                "all_time": expected_unchanged_stats
            },
            "Statistics should not have been reset"
        )

    def test_sample_day_reset(self):
        t = datetime(year=2020, month=6, day=1, hour=23, minute=55)

        def time_src():
            return t

        c = MultiPeriodStatisticsCollector(time_src)
        c.log_sample_record(3, 5, "skip")

        # Check statistics have been logged...
        stats = c.get_sample_statistics()

        expected_stats = {
            "count": 1,
            "algorithm": {
                "skip": {
                    "count": 1,
                    "total_unencoded_size": 5,
                    "total_encoded_size": 3
                }
            }
        }

        self.assertDictEqual(
            stats,
            {
                "today": expected_stats,
                "week": expected_stats,
                "month": expected_stats,
                "all_time": expected_stats
            },
            "Statistics should be logged for current date (and others)"
        )

        # Move on to the next day
        t = datetime(year=2020, month=6, day=2, hour=0, minute=0)

        # Log some statistics. This should cause the day statistics to be cleared before
        # this sample record is logged
        c.log_sample_record(2, 5, "live-diff")

        stats = c.get_sample_statistics()

        expected_stats = {
            "count": 2,
            "algorithm": {
                "skip": {
                    "count": 1,
                    "total_unencoded_size": 5,
                    "total_encoded_size": 3
                },
                "live-diff": {
                    "count": 1,
                    "total_unencoded_size": 5,
                    "total_encoded_size": 2
                }
            }
        }

        self.assertDictEqual(
            stats,
            {
                "today": {
                    "count": 1,
                    "algorithm": {
                        "live-diff": {
                            "count": 1,
                            "total_unencoded_size": 5,
                            "total_encoded_size": 2
                        }
                    }
                },
                "week": expected_stats,
                "month": expected_stats,
                "all_time": expected_stats
            },
            "Statistics should have been cleared for current day without affecting others"
        )

    def test_sample_two_day_resets(self):
        t = datetime(year=2020, month=6, day=1, hour=23, minute=55)

        def time_src():
            return t

        c = MultiPeriodStatisticsCollector(time_src)
        c.log_sample_record(3, 5, "skip")

        # Check statistics have been logged...
        stats = c.get_sample_statistics()

        expected_stats = {
            "count": 1,
            "algorithm": {
                "skip": {
                    "count": 1,
                    "total_unencoded_size": 5,
                    "total_encoded_size": 3
                }
            }
        }

        self.assertDictEqual(
            stats,
            {
                "today": expected_stats,
                "week": expected_stats,
                "month": expected_stats,
                "all_time": expected_stats
            },
            "Statistics should be logged for current date (and others)"
        )

        # Move on to the next day
        t = datetime(year=2020, month=6, day=2, hour=0, minute=0)

        # Log some statistics. This should cause the day statistics to be cleared before
        # this live record is logged
        c.log_sample_record(2, 5, "live-diff")

        stats = c.get_sample_statistics()

        expected_stats = {
            "count": 2,
            "algorithm": {
                "skip": {
                    "count": 1,
                    "total_unencoded_size": 5,
                    "total_encoded_size": 3
                },
                "live-diff": {
                    "count": 1,
                    "total_unencoded_size": 5,
                    "total_encoded_size": 2
                }
            }
        }

        self.assertDictEqual(
            stats,
            {
                "today": {
                    "count": 1,
                    "algorithm": {
                        "live-diff": {
                            "count": 1,
                            "total_unencoded_size": 5,
                            "total_encoded_size": 2
                        }
                    }
                },
                "week": expected_stats,
                "month": expected_stats,
                "all_time": expected_stats
            },
            "Statistics should have been cleared for current day without affecting others"
        )

        # Log some more data
        t = datetime(year=2020, month=6, day=2, hour=0, minute=5)

        # Log some statistics. This should cause the day statistics to be cleared again
        # before this live record is logged
        c.log_sample_record(1, 5, "skip")

        stats = c.get_sample_statistics()

        expected_stats = {
            "count": 3,
            "algorithm": {
                "skip": {
                    "count": 2,
                    "total_unencoded_size": 10,
                    "total_encoded_size": 4
                },
                "live-diff": {
                    "count": 1,
                    "total_unencoded_size": 5,
                    "total_encoded_size": 2
                }
            }
        }

        self.assertDictEqual(
            stats,
            {
                "today": {
                    "count": 2,
                    "algorithm": {
                        "live-diff": {
                            "count": 1,
                            "total_unencoded_size": 5,
                            "total_encoded_size": 2
                        },
                        "skip": {
                            "count": 1,
                            "total_unencoded_size": 5,
                            "total_encoded_size": 1
                        }
                    }
                },
                "week": expected_stats,
                "month": expected_stats,
                "all_time": expected_stats
            },
            "Statistics should not have been reset"
        )

        # Move on to another day
        t = datetime(year=2020, month=6, day=3, hour=0, minute=0)

        # Log some statistics. This should cause the day statistics to be cleared again
        # before this live record is logged
        c.log_sample_record(2, 5, "live-diff")

        stats = c.get_sample_statistics()

        expected_stats = {
            "count": 4,
            "algorithm": {
                "skip": {
                    "count": 2,
                    "total_unencoded_size": 10,
                    "total_encoded_size": 4
                },
                "live-diff": {
                    "count": 2,
                    "total_unencoded_size": 10,
                    "total_encoded_size": 4
                }
            }
        }

        self.assertDictEqual(
            stats,
            {
                "today": {
                    "count": 1,
                    "algorithm": {
                        "live-diff": {
                            "count": 1,
                            "total_unencoded_size": 5,
                            "total_encoded_size": 2
                        }
                    }
                },
                "week": expected_stats,
                "month": expected_stats,
                "all_time": expected_stats
            },
            "Statistics should have been cleared again for current day without affecting others"
        )

    def test_sample_month_reset(self):
        t = datetime(year=2020, month=6, day=30, hour=23, minute=55)

        def time_src():
            return t

        c = MultiPeriodStatisticsCollector(time_src)
        c.log_sample_record(3, 5, "skip")

        # Check statistics have been logged...
        stats = c.get_sample_statistics()

        expected_stats = {
            "count": 1,
            "algorithm": {
                "skip": {
                    "count": 1,
                    "total_unencoded_size": 5,
                    "total_encoded_size": 3
                }
            }
        }

        self.assertDictEqual(
            stats,
            {
                "today": expected_stats,
                "week": expected_stats,
                "month": expected_stats,
                "all_time": expected_stats
            },
            "Statistics should be logged for current date (and others)"
        )

        # Move on to the next day (and next month)
        t = datetime(year=2020, month=7, day=1, hour=0, minute=0)

        # Log some statistics. This should cause the day & month statistics to be cleared before
        # this live record is logged
        c.log_sample_record(2, 5, "live-diff")

        stats = c.get_sample_statistics()

        expected_reset_stats = {
                    "count": 1,
                    "algorithm": {
                        "live-diff": {
                            "count": 1,
                            "total_unencoded_size": 5,
                            "total_encoded_size": 2
                        }
                    }
                }
        expected_unchanged_stats = {
            "count": 2,
            "algorithm": {
                "skip": {
                    "count": 1,
                    "total_unencoded_size": 5,
                    "total_encoded_size": 3
                },
                "live-diff": {
                    "count": 1,
                    "total_unencoded_size": 5,
                    "total_encoded_size": 2
                }
            }
        }

        self.assertDictEqual(
            stats,
            {
                "today": expected_reset_stats,
                "week": expected_unchanged_stats,
                "month": expected_reset_stats,
                "all_time": expected_unchanged_stats
            },
            "Statistics should have been cleared for current day without affecting others"
        )

        # Log some more data. This shouldn't reset anything
        t = datetime(year=2020, month=7, day=1, hour=0, minute=5)
        c.log_sample_record(2, 5, "live-diff")

        stats = c.get_sample_statistics()

        expected_reset_stats = {
            "count": 2,
            "algorithm": {
                "live-diff": {
                    "count": 2,
                    "total_unencoded_size": 10,
                    "total_encoded_size": 4
                }
            }
        }
        expected_unchanged_stats = {
            "count": 3,
            "algorithm": {
                "skip": {
                    "count": 1,
                    "total_unencoded_size": 5,
                    "total_encoded_size": 3
                },
                "live-diff": {
                    "count": 2,
                    "total_unencoded_size": 10,
                    "total_encoded_size": 4
                }
            }
        }

        self.assertDictEqual(
            stats,
            {
                "today": expected_reset_stats,
                "week": expected_unchanged_stats,
                "month": expected_reset_stats,
                "all_time": expected_unchanged_stats
            },
            "Statistics should not have been reset"
        )

        # Move on to the next month
        t = datetime(year=2020, month=8, day=1, hour=0, minute=0)

        # Log some statistics. This should cause the day, week & month statistics to be cleared
        # before this live record is logged
        c.log_sample_record(2, 5, "live-diff")

        stats = c.get_sample_statistics()

        expected_reset_stats = {
            "count": 1,
            "algorithm": {
                "live-diff": {
                    "count": 1,
                    "total_unencoded_size": 5,
                    "total_encoded_size": 2
                }
            }
        }
        expected_unchanged_stats = {
            "count": 4,
            "algorithm": {
                "skip": {
                    "count": 1,
                    "total_unencoded_size": 5,
                    "total_encoded_size": 3
                },
                "live-diff": {
                    "count": 3,
                    "total_unencoded_size": 15,
                    "total_encoded_size": 6
                }
            }
        }

        self.assertDictEqual(
            stats,
            {
                "today": expected_reset_stats,
                "week": expected_reset_stats,
                "month": expected_reset_stats,
                "all_time": expected_unchanged_stats
            },
            "Statistics should have been reset for day and month"
        )

    def test_sample_week_reset(self):
        t = datetime(year=2020, month=5, day=31, hour=23, minute=55)

        def time_src():
            return t

        c = MultiPeriodStatisticsCollector(time_src)
        c.log_sample_record(3, 5, "skip")

        # Check statistics have been logged...
        stats = c.get_sample_statistics()

        expected_stats = {
            "count": 1,
            "algorithm": {
                "skip": {
                    "count": 1,
                    "total_unencoded_size": 5,
                    "total_encoded_size": 3
                }
            }
        }

        self.assertDictEqual(
            stats,
            {
                "today": expected_stats,
                "week": expected_stats,
                "month": expected_stats,
                "all_time": expected_stats
            },
            "Statistics should be logged for current date (and others)"
        )

        # Move on to the next day
        t = datetime(year=2020, month=6, day=1, hour=0, minute=0)

        # Log some statistics. This should cause the day, week and month statistics to be cleared
        # before this live record is logged
        c.log_sample_record(2, 5, "live-diff")

        stats = c.get_sample_statistics()

        expected_unchanged_stats = {
            "count": 2,
            "algorithm": {
                "skip": {
                    "count": 1,
                    "total_unencoded_size": 5,
                    "total_encoded_size": 3
                },
                "live-diff": {
                    "count": 1,
                    "total_unencoded_size": 5,
                    "total_encoded_size": 2
                }
            }
        }

        expected_reset_stats = {
                    "count": 1,
                    "algorithm": {
                        "live-diff": {
                            "count": 1,
                            "total_unencoded_size": 5,
                            "total_encoded_size": 2
                        }
                    }
                }

        self.assertDictEqual(
            stats,
            {
                "today": expected_reset_stats,
                "week": expected_reset_stats,
                "month": expected_reset_stats,
                "all_time": expected_unchanged_stats
            },
            "Statistics should have been cleared for current day and week without affecting others"
        )

        t = datetime(year=2020, month=6, day=1, hour=0, minute=5)

        # Log some statistics. This shouldn't cause anything to be reset
        c.log_sample_record(2, 5, "live-diff")

        stats = c.get_sample_statistics()

        expected_unchanged_stats = {
            "count": 3,
            "algorithm": {
                "skip": {
                    "count": 1,
                    "total_unencoded_size": 5,
                    "total_encoded_size": 3
                },
                "live-diff": {
                    "count": 2,
                    "total_unencoded_size": 10,
                    "total_encoded_size": 4
                }
            }
        }

        expected_reset_stats = {
            "count": 2,
            "algorithm": {
                "live-diff": {
                    "count": 2,
                    "total_unencoded_size": 10,
                    "total_encoded_size": 4
                }
            }
        }

        self.assertDictEqual(
            stats,
            {
                "today": expected_reset_stats,
                "week": expected_reset_stats,
                "month": expected_reset_stats,
                "all_time": expected_unchanged_stats
            },
            "Statistics should not have been reset"
        )

    def test_packet_day_reset(self):
        t = datetime(year=2020, month=6, day=1, hour=23, minute=55)

        def time_src():
            return t

        c = MultiPeriodStatisticsCollector(time_src)
        c.log_packet_transmission("test", 5)

        # Check statistics have been logged...
        stats = c.get_packet_statistics()

        expected_stats = {
            "count": 1,
            "type": {
                "test": {
                    "count": 1,
                    "total_size": 5
                }
            }
        }

        self.assertDictEqual(
            stats,
            {
                "today": expected_stats,
                "week": expected_stats,
                "month": expected_stats,
                "all_time": expected_stats
            },
            "Statistics should be logged for current date (and others)"
        )

        # Move on to the next day
        t = datetime(year=2020, month=6, day=2, hour=0, minute=0)

        # Log some statistics. This should cause the day statistics to be cleared before
        # this packet is logged
        c.log_packet_transmission("test", 5)

        stats = c.get_packet_statistics()

        expected_stats = {
            "count": 2,
            "type": {
                "test": {
                    "count": 2,
                    "total_size": 10
                },
            }
        }

        self.assertDictEqual(
            stats,
            {
                "today": {
                    "count": 1,
                    "type": {
                        "test": {
                            "count": 1,
                            "total_size": 5
                        }
                    }
                },
                "week": expected_stats,
                "month": expected_stats,
                "all_time": expected_stats
            },
            "Statistics should have been cleared for current day without affecting others"
        )

    def test_packet_two_day_resets(self):
        t = datetime(year=2020, month=6, day=1, hour=23, minute=55)

        def time_src():
            return t

        c = MultiPeriodStatisticsCollector(time_src)
        c.log_packet_transmission("test", 5)

        # Check statistics have been logged...
        stats = c.get_packet_statistics()

        expected_stats = {
            "count": 1,
            "type": {
                "test": {
                    "count": 1,
                    "total_size": 5
                }
            }
        }

        self.assertDictEqual(
            stats,
            {
                "today": expected_stats,
                "week": expected_stats,
                "month": expected_stats,
                "all_time": expected_stats
            },
            "Statistics should be logged for current date (and others)"
        )

        # Move on to the next day
        t = datetime(year=2020, month=6, day=2, hour=0, minute=0)

        # Log some statistics. This should cause the day statistics to be cleared before
        # this packet is logged
        c.log_packet_transmission("test", 5)

        stats = c.get_packet_statistics()

        expected_stats = {
            "count": 2,
            "type": {
                "test": {
                    "count": 2,
                    "total_size": 10
                }
            }
        }

        self.assertDictEqual(
            stats,
            {
                "today": {
                    "count": 1,
                    "type": {
                        "test": {
                            "count": 1,
                            "total_size": 5
                        }
                    }
                },
                "week": expected_stats,
                "month": expected_stats,
                "all_time": expected_stats
            },
            "Statistics should have been cleared for current day without affecting others"
        )

        # Log some more data
        t = datetime(year=2020, month=6, day=2, hour=0, minute=5)

        # Log some statistics. This should cause the day statistics to be cleared again
        # before this packet is logged
        c.log_packet_transmission("test", 5)

        stats = c.get_packet_statistics()

        expected_stats = {
            "count": 3,
            "type": {
                "test": {
                    "count": 3,
                    "total_size": 15
                }
            }
        }

        self.assertDictEqual(
            stats,
            {
                "today": {
                    "count": 2,
                    "type": {
                        "test": {
                            "count": 2,
                            "total_size": 10
                        }
                    }
                },
                "week": expected_stats,
                "month": expected_stats,
                "all_time": expected_stats
            },
            "Statistics should not have been reset"
        )

        # Move on to another day
        t = datetime(year=2020, month=6, day=3, hour=0, minute=0)

        # Log some statistics. This should cause the day statistics to be cleared again
        # before this packet is logged
        c.log_packet_transmission("test", 5)

        stats = c.get_packet_statistics()

        expected_stats = {
            "count": 4,
            "type": {
                "test": {
                    "count": 4,
                    "total_size": 20
                }
            }
        }

        self.assertDictEqual(
            stats,
            {
                "today": {
                    "count": 1,
                    "type": {
                        "test": {
                            "count": 1,
                            "total_size": 5
                        }
                    }
                },
                "week": expected_stats,
                "month": expected_stats,
                "all_time": expected_stats
            },
            "Statistics should have been cleared again for current day without affecting others"
        )

    def test_packet_month_reset(self):
        t = datetime(year=2020, month=6, day=30, hour=23, minute=55)

        def time_src():
            return t

        c = MultiPeriodStatisticsCollector(time_src)
        c.log_packet_transmission("test", 5)

        # Check statistics have been logged...
        stats = c.get_packet_statistics()

        expected_stats = {
            "count": 1,
            "type": {
                "test": {
                    "count": 1,
                    "total_size": 5
                }
            }
        }

        self.assertDictEqual(
            stats,
            {
                "today": expected_stats,
                "week": expected_stats,
                "month": expected_stats,
                "all_time": expected_stats
            },
            "Statistics should be logged for current date (and others)"
        )

        # Move on to the next day (and next month)
        t = datetime(year=2020, month=7, day=1, hour=0, minute=0)

        # Log some statistics. This should cause the day & month statistics to be cleared before
        # this packet is logged
        c.log_packet_transmission("test", 5)

        stats = c.get_packet_statistics()

        expected_reset_stats = {
                    "count": 1,
                    "type": {
                        "test": {
                            "count": 1,
                            "total_size": 5
                        }
                    }
                }
        expected_unchanged_stats = {
            "count": 2,
            "type": {
                "test": {
                    "count": 2,
                    "total_size": 10
                }
            }
        }

        self.assertDictEqual(
            stats,
            {
                "today": expected_reset_stats,
                "week": expected_unchanged_stats,
                "month": expected_reset_stats,
                "all_time": expected_unchanged_stats
            },
            "Statistics should have been cleared for current day without affecting others"
        )

        # Log some more data. This shouldn't reset anything
        t = datetime(year=2020, month=7, day=1, hour=0, minute=5)
        c.log_packet_transmission("test", 5)

        stats = c.get_packet_statistics()

        expected_reset_stats = {
                    "count": 2,
                    "type": {
                        "test": {
                            "count": 2,
                            "total_size": 10
                        }
                    }
                }
        expected_unchanged_stats = {
            "count": 3,
            "type": {
                "test": {
                    "count": 3,
                    "total_size": 15
                }
            }
        }
        self.assertDictEqual(
            stats,
            {
                "today": expected_reset_stats,
                "week": expected_unchanged_stats,
                "month": expected_reset_stats,
                "all_time": expected_unchanged_stats
            },
            "Statistics should not have been reset"
        )

        # Move on to the next month
        t = datetime(year=2020, month=8, day=1, hour=0, minute=0)

        # Log some statistics. This should cause the day, week & month statistics to be cleared
        # before this packet is logged
        c.log_packet_transmission("test", 5)

        stats = c.get_packet_statistics()

        expected_reset_stats = {
                    "count": 1,
                    "type": {
                        "test": {
                            "count": 1,
                            "total_size": 5
                        }
                    }
                }
        expected_unchanged_stats = {
            "count": 4,
            "type": {
                "test": {
                    "count": 4,
                    "total_size": 20
                }
            }
        }

        self.assertDictEqual(
            stats,
            {
                "today": expected_reset_stats,
                "week": expected_reset_stats,
                "month": expected_reset_stats,
                "all_time": expected_unchanged_stats
            },
            "Statistics should have been reset for day and month"
        )

    def test_packet_week_reset(self):
        t = datetime(year=2020, month=5, day=31, hour=23, minute=55)

        def time_src():
            return t

        c = MultiPeriodStatisticsCollector(time_src)
        c.log_packet_transmission("test", 5)

        # Check statistics have been logged...
        stats = c.get_packet_statistics()

        expected_stats = {
            "count": 1,
            "type": {
                "test": {
                    "count": 1,
                    "total_size": 5
                }
            }
        }

        self.assertDictEqual(
            stats,
            {
                "today": expected_stats,
                "week": expected_stats,
                "month": expected_stats,
                "all_time": expected_stats
            },
            "Statistics should be logged for current date (and others)"
        )

        # Move on to the next day
        t = datetime(year=2020, month=6, day=1, hour=0, minute=0)

        # Log some statistics. This should cause the day, week and month statistics to be cleared
        # before this packet is logged
        c.log_packet_transmission("test", 5)

        stats = c.get_packet_statistics()

        expected_unchanged_stats = {
            "count": 2,
            "type": {
                "test": {
                    "count": 2,
                    "total_size": 10
                }
            }
        }

        expected_reset_stats = {
                    "count": 1,
                    "type": {
                        "test": {
                            "count": 1,
                            "total_size": 5
                        }
                    }
                }

        self.assertDictEqual(
            stats,
            {
                "today": expected_reset_stats,
                "week": expected_reset_stats,
                "month": expected_reset_stats,
                "all_time": expected_unchanged_stats
            },
            "Statistics should have been cleared for current day and week without affecting others"
        )

        t = datetime(year=2020, month=6, day=1, hour=0, minute=5)

        # Log some statistics. This shouldn't cause anything to be reset
        c.log_packet_transmission("test", 5)

        stats = c.get_packet_statistics()

        expected_unchanged_stats = {
            "count": 3,
            "type": {
                "test": {
                    "count": 3,
                    "total_size": 15
                }
            }
        }

        expected_reset_stats = {
                    "count": 2,
                    "type": {
                        "test": {
                            "count": 2,
                            "total_size": 10
                        }
                    }
                }

        self.assertDictEqual(
            stats,
            {
                "today": expected_reset_stats,
                "week": expected_reset_stats,
                "month": expected_reset_stats,
                "all_time": expected_unchanged_stats
            },
            "Statistics should not have been reset"
        )