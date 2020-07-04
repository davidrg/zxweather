select
  datetime(s.time_stamp, 'unixepoch', 'localtime') as time_stamp,
  ds.solar_radiation as solar_radiation,
  0 as max_solar_radiation,
  0 as bright_sunshine_hours,
  stn.sample_interval * 60 as sample_interval,
  id.date as image_link
from sample s
inner join davis_sample ds on ds.sample_id = s.sample_id
inner join station stn on stn.station_id = s.station_id
left outer join image_source i on stn.station_id = i.station
left outer join image_dates id on id.date = substr(datetime(s.time_stamp, 'unixepoch', 'localtime'),0,11)
                              and id.image_source_id = i.id
where lower(stn.code) = lower(:stationCode)
  and s.time_stamp between (:start_t) and (:end_t)
order by time_stamp desc;
