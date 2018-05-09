 select (:at_time)::time as time,
        extract(hour from (:offset)::timestamp)::int as time_offset