select
  s.time_stamp as time_stamp,
  ds.solar_radiation as solar_radiation,
  0 as max_solar_radiation,
  0 as bright_sunshine_hours,
  stn.sample_interval as sample_interval,
  image_dates.date as image_link
from sample s
inner join davis_sample ds on ds.sample_id = s.sample_id
inner join station stn on stn.station_id = s.station_id
left outer join image_source i on stn.station_id = i.station_id
left outer join (
    select time_stamp::date as date,
           image_source_id
    from image
    group by time_stamp::date, image_source_id
    ) as image_dates on image_dates.date = s.time_stamp::date
                        and image_dates.image_source_id = i.image_source_id
 where lower(stn.code) = lower(:stationCode)
   and s.time_stamp between to_timestamp(:start_t) and to_timestamp(:end_t)
order by time_stamp desc  
