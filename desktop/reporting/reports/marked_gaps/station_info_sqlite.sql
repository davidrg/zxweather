select stn.title,
       stn.archived,
       datetime(stn.archived_time, 'unixepoch', 'localtime') as archived_time,
       stn.archived_message,
       case when exists (select 1 from sample_gap where station_id = stn.station_id) then 1 else 0 end as has_known_gaps
from station stn
where lower(stn.code) = lower(:stationCode)
