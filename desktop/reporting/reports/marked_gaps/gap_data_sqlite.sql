select datetime(sg.start_time, 'unixepoch', 'localtime') as start_time,
       datetime(sg.end_time, 'unixepoch', 'localtime') as end_time,
       sg.missing_sample_count * stn.sample_interval as gap_length_minutes,
       sg.missing_sample_count,
       sg.label
from sample_gap sg
inner join station stn on stn.station_id = sg.station_id
where lower(stn.code) = lower(:stationCode)