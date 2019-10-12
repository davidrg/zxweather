with parameters as (
    select upper((:stationCode)::varchar) as station_code,
           (:temp_sensor_value)::varchar  as temperature_sensor,
           (:temp_threshold)::float       as temp_threshold,
           (:start)::date                 as start_date, -- inclusive
           (:end)::date                   as end_date -- inclusive
),
     sensor_data as (
         select s.time_stamp::date                as date,
                case
                    when p.temperature_sensor = 'indoor_temperature' then s.indoor_temperature
                    when p.temperature_sensor = 'outdoor_temperature' then s.temperature
                    when p.temperature_sensor = 'leaf_temperature_1' then ds.leaf_temperature_1
                    when p.temperature_sensor = 'leaf_temperature_2' then ds.leaf_temperature_2
                    when p.temperature_sensor = 'soil_temperature_1' then ds.soil_temperature_1
                    when p.temperature_sensor = 'soil_temperature_2' then ds.soil_temperature_2
                    when p.temperature_sensor = 'soil_temperature_3' then ds.soil_temperature_3
                    when p.temperature_sensor = 'soil_temperature_4' then ds.soil_temperature_4
                    when p.temperature_sensor = 'extra_temperature_1' then ds.extra_temperature_1
                    when p.temperature_sensor = 'extra_temperature_2' then ds.extra_temperature_2
                    when p.temperature_sensor = 'extra_temperature_3' then ds.extra_temperature_3
                    else null
                    end                           as temperature,
                stn.sample_interval / 60.0 / 60.0 as sample_interval -- in hours
         from parameters p,
              sample s
                  inner join davis_sample ds on ds.sample_id = s.sample_id
                  inner join station stn on s.station_id = stn.station_id
         where upper(stn.code) = p.station_code
           and s.time_stamp::date >= p.start_date
           and s.time_stamp::date <= p.end_date
     ),
     ranges as (
         select sd.date,
                sd.temperature,
                case when sd.temperature >= p.temp_threshold then true else false end as temp_in_range,
                sd.sample_interval
         from sensor_data sd,
              parameters p
     ),
     temp_hours as (
         select r.date,
                case when r.temp_in_range then r.sample_interval else 0 end as temp,
                r.temperature
         from ranges r
     )
select wh.date,
       (sum(temp))      as temp_hours,
       min(temperature) as min_temp,
       max(temperature) as max_temp
from temp_hours wh
group by wh.date
order by wh.date desc
;