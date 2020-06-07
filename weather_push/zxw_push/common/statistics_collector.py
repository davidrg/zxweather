import datetime


class ClientStatisticsCollector(object):
    """
    Collects statistics.
    """
    def __init__(self):
        self.live_records = 0
        self.live_algorithms = dict()

        self.sample_records = 0
        self.sample_algorithms = dict()

        self.packets = 0
        self.packet_types = dict()

    def log_live_record(self, encoded_size, unencoded_size, algorithm):
        """
        Logs statistics for a transmitted live record

        :param encoded_size: Size of the encoded data (this is the total size of the fields being sent)
        :param unencoded_size: Size of the unencoded data (total size of all fields)
        :param algorithm: encoding algorithm used
        """

        self.live_records += 1

        if algorithm not in self.live_algorithms:
            self.live_algorithms[algorithm] = {
                "count": 0,
                "total_unencoded_size": 0,
                "total_encoded_size": 0
            }

        self.live_algorithms[algorithm]["count"] += 1
        self.live_algorithms[algorithm]["total_unencoded_size"] += unencoded_size
        self.live_algorithms[algorithm]["total_encoded_size"] += encoded_size

    def log_sample_record(self, encoded_size, unencoded_size, algorithm):
        """
        Logs statistics for a transmitted sample record

        :param encoded_size: Size of the encoded data (this is the total size of the fields being sent)
        :param unencoded_size: Size of the unencoded data (total size of all fields)
        :param algorithm: encoding algorithm used
        """

        self.sample_records += 1

        if algorithm not in self.sample_algorithms:
            self.sample_algorithms[algorithm] = {
                "count": 0,
                "total_unencoded_size": 0,
                "total_encoded_size": 0
            }

        self.sample_algorithms[algorithm]["count"] += 1
        self.sample_algorithms[algorithm]["total_unencoded_size"] += unencoded_size
        self.sample_algorithms[algorithm]["total_encoded_size"] += encoded_size

    def log_packet_transmission(self, packet_type, packet_size):
        """
        Logs statistics for a transmitted packet.

        :param packet_type: Type of packet transmitted
        :param packet_size: Total size of the packet
        """

        self.packets += 1

        if packet_type not in self.packet_types:
            self.packet_types[packet_type] = {
                "count": 0,
                "total_size": 0
            }

        self.packet_types[packet_type]["count"] += 1
        self.packet_types[packet_type]["total_size"] += packet_size

    def get_live_statistics(self):
        return {
            "count": self.live_records,
            "algorithm": self.live_algorithms
        }

    def get_sample_statistics(self):
        return {
            "count": self.sample_records,
            "algorithm": self.sample_algorithms
        }

    def get_packet_statistics(self):
        return {
            "count": self.packets,
            "type": self.packet_types
        }

    def reset_statistics(self):
        self.live_records = 0
        self.live_algorithms = dict()
        self.sample_records = 0
        self.sample_algorithms = dict()
        self.packets = 0
        self.packet_types = dict()


def console_log_function(msg):
    print(msg)


class MultiPeriodClientStatisticsCollector(object):
    """
    Collects statistics for today, this week, this month and all time.
    """
    def __init__(self, time_source=datetime.datetime.now, log_function=console_log_function):
        self.today = ClientStatisticsCollector()
        self.week = ClientStatisticsCollector()
        self.month = ClientStatisticsCollector()
        self.all_time = ClientStatisticsCollector()
        self.now = time_source

        date = time_source().date()
        self.this_date = date
        self.this_month = date.month
        self.this_week = date.isocalendar()[1]

        self.log = log_function

    def _check_reset(self):
        date = self.now().date()
        week = date.isocalendar()[1]

        if date != self.this_date:
            self._log_statistics("end of day {0}".format(self.this_date.isoformat()), self.today)

            # Also log the all-time statistics daily
            self._log_statistics("all time", self.all_time)

            self.today.reset_statistics()
            self.this_date = date

        if week != self.this_week:
            self._log_statistics("end of week {0}".format(self.this_week), self.today)
            self.week.reset_statistics()
            self.this_week = week

        if date.month != self.this_month:
            self._log_statistics("end of month {0}".format(self.this_month), self.today)
            self.month.reset_statistics()
            self.this_month = date.month

    def log_live_record(self, encoded_size, unencoded_size, algorithm):
        self._check_reset()

        self.today.log_live_record(encoded_size, unencoded_size, algorithm)
        self.week.log_live_record(encoded_size, unencoded_size, algorithm)
        self.month.log_live_record(encoded_size, unencoded_size, algorithm)
        self.all_time.log_live_record(encoded_size, unencoded_size, algorithm)

    def log_sample_record(self, encoded_size, unencoded_size, algorithm):
        self._check_reset()

        self.today.log_sample_record(encoded_size, unencoded_size, algorithm)
        self.week.log_sample_record(encoded_size, unencoded_size, algorithm)
        self.month.log_sample_record(encoded_size, unencoded_size, algorithm)
        self.all_time.log_sample_record(encoded_size, unencoded_size, algorithm)

    def log_packet_transmission(self, packet_type, packet_size):
        self._check_reset()

        self.today.log_packet_transmission(packet_type, packet_size)
        self.week.log_packet_transmission(packet_type, packet_size)
        self.month.log_packet_transmission(packet_type, packet_size)
        self.all_time.log_packet_transmission(packet_type, packet_size)

    def get_live_statistics(self):
        return {
            "today": self.today.get_live_statistics(),
            "week": self.week.get_live_statistics(),
            "month": self.month.get_live_statistics(),
            "all_time": self.all_time.get_live_statistics(),
        }

    def get_sample_statistics(self):
        return {
            "today": self.today.get_sample_statistics(),
            "week": self.week.get_sample_statistics(),
            "month": self.month.get_sample_statistics(),
            "all_time": self.all_time.get_sample_statistics(),
        }

    def get_packet_statistics(self):
        return {
            "today": self.today.get_packet_statistics(),
            "week": self.week.get_packet_statistics(),
            "month": self.month.get_packet_statistics(),
            "all_time": self.all_time.get_packet_statistics(),
        }

    def _log_statistics(self, period, stats):
        """
        Dumps the supplied statistics for the specified period to the logs
        :param period: Human-friendly description of the period
        :param stats: Statistics to log
        """
        live_stats = stats.get_live_statistics()
        sample_stats = stats.get_sample_statistics()
        packet_stats = stats.get_packet_statistics()

        live_data_saved = 0
        sample_data_saved = 0

        live_by_algorithm = ""
        for algorithm in live_stats["algorithm"].keys():
            total_unencoded = live_stats["algorithm"][algorithm]["total_unencoded_size"]
            total_encoded = live_stats["algorithm"][algorithm]["total_encoded_size"]

            live_by_algorithm += "\t{algorithm}\n\t\tcount: {count}" \
                                 "\n\t\ttotal unencoded size: {total_unencoded} bytes" \
                                 "\n\t\ttotal encoded size: {total_encoded} bytes" \
                                 "\n\t\ttotal saved: {total_saved} bytes\n".format(
                algorithm=algorithm,
                count=live_stats["algorithm"][algorithm]["count"],
                total_unencoded=total_unencoded,
                total_encoded=total_encoded,
                total_saved=total_unencoded-total_encoded
            )
            live_data_saved += total_unencoded-total_encoded

        sample_by_algorithm = ""
        for algorithm in sample_stats["algorithm"].keys():
            total_unencoded = sample_stats["algorithm"][algorithm]["total_unencoded_size"]
            total_encoded = sample_stats["algorithm"][algorithm]["total_encoded_size"]

            sample_by_algorithm += "\t{algorithm}\n\t\tcount: {count}" \
                                 "\n\t\ttotal unencoded size: {total_unencoded} bytes" \
                                 "\n\t\ttotal encoded size: {total_encoded} bytes" \
                                 "\n\t\ttotal saved: {total_saved} bytes\n".format(
                algorithm=algorithm,
                count=sample_stats["algorithm"][algorithm]["count"],
                total_unencoded=total_unencoded,
                total_encoded=total_encoded,
                total_saved=total_unencoded-total_encoded
            )
            sample_data_saved += total_unencoded-total_encoded

        packets_by_type = ""
        for packet_type in packet_stats["type"].keys():
            packets_by_type += "\t{type}\n\t\tcount: {count}" \
                                 "\n\t\ttotal size: {size}\n".format(
                type=packet_type,
                count=packet_stats["type"][packet_type]["count"],
                size=packet_stats["type"][packet_type]["total_size"]
            )

        self.log("""Statistics for {period}
-------------------------------------------------------------
Total Live Records: {live_count}
Live by encoding algorithm:
{live_by_algorithm}

Total Sample Records: {sample_count}
Samples by encoding algorithm:
{sample_by_algorithm}

Total Packets: {packet_count}
Packets by type:
{packets_by_type}

Total data saved: {total_saved}
    - Live: {live_saved}
    - Samples: {sample_saved}
""".format(period=period,
           live_count=live_stats["count"],
           live_by_algorithm=live_by_algorithm,
           sample_count=sample_stats["count"],
           sample_by_algorithm=sample_by_algorithm,
           packet_count=packet_stats["count"],
           packets_by_type=packets_by_type,
           total_saved=sample_data_saved+live_data_saved,
           live_saved=live_data_saved,
           sample_saved=sample_data_saved
           ))


class ServerStatisticsCollector(object):
    def __init__(self):
        self._sent_packets = dict()
        self._duplicate_images = 0
        self._duplicate_samples = 0
        self._undecodable_live_records = 0
        self._undecodable_sample_records = 0
        self._recovered_live_records = 0

    def log_packet_transmission(self, packet_type, packet_size):
        """
        Logs statistics for a transmitted packet.

        :param packet_type: Type of packet transmitted
        :param packet_size: Total size of the packet
        """

        if packet_type not in self._sent_packets:
            self._sent_packets[packet_type] = {
                "count": 0,
                "total_size": 0
            }

        self._sent_packets[packet_type]["count"] += 1
        self._sent_packets[packet_type]["total_size"] += packet_size

    def log_duplicate_image_receipt(self):
        self._duplicate_images += 1

    def log_duplicate_sample_receipt(self):
        self._duplicate_samples += 1

    def log_undecodable_live_record(self):
        self._undecodable_live_records += 1

    def log_recovered_live_record(self):
        self._recovered_live_records += 1

    def log_undecodable_sample_record(self):
        self._undecodable_sample_records += 1

    def statistics(self):
        stats = {
            "sent_packets": self._sent_packets,
            "duplicate_images": self._duplicate_images,
            "duplicate_samples": self._duplicate_samples,
            "undecodable_live_records": self._undecodable_live_records,
            "undecodable_sample_records": self._undecodable_sample_records,
            "recovered_live_records": self._recovered_live_records,
        }
        return stats

    def reset_statistics(self):
        self._sent_packets = dict()
        self._duplicate_images = 0
        self._duplicate_samples = 0
        self._undecodable_live_records = 0
        self._undecodable_sample_records = 0
        self._recovered_live_records = 0


class MultiPeriodServerStatisticsCollector(object):
    """
    Collects statistics for today, this week, this month and all time.
    """
    def __init__(self, time_source=datetime.datetime.now, log_function=console_log_function):
        self.today = ServerStatisticsCollector()
        self.week = ServerStatisticsCollector()
        self.month = ServerStatisticsCollector()
        self.all_time = ServerStatisticsCollector()
        self.now = time_source

        date = time_source().date()
        self.this_date = date
        self.this_month = date.month
        self.this_week = date.isocalendar()[1]

        self.log = log_function

    def _check_reset(self):
        date = self.now().date()
        week = date.isocalendar()[1]

        if date != self.this_date:
            self._log_statistics("end of day {0}".format(self.this_date.isoformat()), self.today)

            # Also log the all-time statistics daily
            self._log_statistics("all time", self.all_time)

            self.today.reset_statistics()
            self.this_date = date

        if week != self.this_week:
            self._log_statistics("end of week {0}".format(self.this_week), self.today)
            self.week.reset_statistics()
            self.this_week = week

        if date.month != self.this_month:
            self._log_statistics("end of month {0}".format(self.this_month), self.today)
            self.month.reset_statistics()
            self.this_month = date.month

    def log_packet_transmission(self, packet_type, packet_size):
       self._check_reset()

       self.today.log_packet_transmission(packet_type, packet_size)
       self.week.log_packet_transmission(packet_type, packet_size)
       self.month.log_packet_transmission(packet_type, packet_size)
       self.all_time.log_packet_transmission(packet_type, packet_size)

    def log_duplicate_image_receipt(self):
        self._check_reset()

        self.today.log_duplicate_image_receipt()
        self.week.log_duplicate_image_receipt()
        self.month.log_duplicate_image_receipt()
        self.all_time.log_duplicate_image_receipt()

    def log_duplicate_sample_receipt(self):
        self._check_reset()

        self.today.log_duplicate_sample_receipt()
        self.week.log_duplicate_sample_receipt()
        self.month.log_duplicate_sample_receipt()
        self.all_time.log_duplicate_sample_receipt()

    def log_undecodable_live_record(self):
        self._check_reset()

        self.today.log_undecodable_live_record()
        self.week.log_undecodable_live_record()
        self.month.log_undecodable_live_record()
        self.all_time.log_undecodable_live_record()

    def log_recovered_live_record(self):
        self._check_reset()

        self.today.log_recovered_live_record()
        self.week.log_recovered_live_record()
        self.month.log_recovered_live_record()
        self.all_time.log_recovered_live_record()

    def log_undecodable_sample_record(self):
        self._check_reset()

        self.today.log_undecodable_sample_record()
        self.week.log_undecodable_sample_record()
        self.month.log_undecodable_sample_record()
        self.all_time.log_undecodable_sample_record()

    def statistics(self):
        stats = {
            "day": self.today.statistics(),
            "week": self.week.statistics(),
            "month": self.month.statistics(),
            "all_time": self.all_time.statistics()
        }
        return stats

    def reset_statistics(self):
        self.today.reset_statistics()
        self.week.reset_statistics()
        self.month.reset_statistics()
        self.all_time.reset_statistics()

    def _log_statistics(self, period, stats):
        """
                Dumps the supplied statistics for the specified period to the logs
                :param period: Human-friendly description of the period
                :param stats: Statistics to log
                """
        stats = stats.statistics()

        packets_by_type = ""
        total_packets = 0
        for packet_type in stats["sent_packets"].keys():
            packets_by_type += "\t{type}\n\t\tcount: {count}" \
                               "\n\t\ttotal size: {size}\n".format(
                type=packet_type,
                count=stats["sent_packets"][packet_type]["count"],
                size=stats["sent_packets"][packet_type]["total_size"]
            )
            total_packets += stats["sent_packets"][packet_type]["count"]

        self.log("""Statistics for {period}
        -------------------------------------------------------------
        Undecodable Live Records: {undecodable_live_count}
        Recovered Live Records: {recovered_live_count}
        Undecodable Sample Records: {undecodable_sample_count}
        
        Duplicate Images Received: {duplicate_images}

        Total Packets: {packet_count}
        Packets by type:
        {packets_by_type}
        """.format(period=period,
                   undecodable_live_count=stats["undecodable_live_records"],
                   undecodable_sample_count=stats["undecodable_sample_records"],
                   recovered_live_count=stats["recovered_live_records"],
                   duplicate_images=stats["duplicate_images"],
                   packet_count=total_packets,
                   packets_by_type=packets_by_type
                   ))
