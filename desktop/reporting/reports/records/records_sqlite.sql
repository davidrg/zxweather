select data.station_id,
    data.rows_processed as rows_processed,
    round(data.total_rainfall, 1) as total_rainfall,
    data.max_gust_wind_speed as max_gust_wind_speed,
    datetime(max(data.max_gust_wind_speed_ts), 'unixepoch') as max_gust_wind_speed_ts,
    data.max_average_wind_speed as max_average_wind_speed,
    datetime(max(data.max_average_wind_speed_ts), 'unixepoch') as max_average_wind_speed_ts,
    data.min_absolute_pressure as min_absolute_pressure,
    datetime(max(data.min_absolute_pressure_ts), 'unixepoch') as min_absolute_pressure_ts,
    data.max_absolute_pressure as max_absolute_pressure,
    datetime(max(data.max_absolute_pressure_ts), 'unixepoch') as max_absolute_pressure_ts,
    round(data.min_apparent_temperature, 1) as min_apparent_temperature,
    datetime(max(data.min_apparent_temperature_ts), 'unixepoch') as min_apparent_temperature_ts,
    round(data.max_apparent_temperature, 1) as max_apparent_temperature,
    datetime(max(data.max_apparent_temperature_ts), 'unixepoch') as max_apparent_temperature_ts,
    round(data.min_wind_chill, 1) as min_wind_chill,
    datetime(max(data.min_wind_chill_ts), 'unixepoch') as min_wind_chill_ts,
    round(data.max_wind_chill, 1) as max_wind_chill,
    datetime(max(data.max_wind_chill_ts), 'unixepoch') as max_wind_chill_ts,
    round(data.min_dew_point, 1) as min_dew_point,
    datetime(max(data.min_dew_point_ts), 'unixepoch') as min_dew_point_ts,
    round(data.max_dew_point, 1) as max_dew_point,
    datetime(max(data.max_dew_point_ts), 'unixepoch') as max_dew_point_ts,
    data.min_temperature as min_temperature,
    datetime(max(data.min_temperature_ts), 'unixepoch') as min_temperature_ts,
    data.max_temperature as max_temperature,
    datetime(max(data.max_temperature_ts), 'unixepoch') as max_temperature_ts,
    data.min_humidity as min_humidity,
    datetime(max(data.min_humidity_ts), 'unixepoch') as min_humidity_ts,
    data.max_humidity as max_humidity,
    datetime(max(data.max_humidity_ts), 'unixepoch') as max_humidity_ts,
    data.max_solar_radiation as max_solar_radiation,
    datetime(max(data.max_solar_radiation_ts), 'unixepoch') as max_solar_radiation_ts,
    data.max_uv_index as max_uv_index,
    datetime(max(data.max_uv_index_ts), 'unixepoch') as max_uv_index_ts,
    data.max_rain_rate as max_rain_rate,
    datetime(max(data.max_rain_rate_ts), 'unixepoch') as max_rain_rate_ts,
    data.max_evapotranspiration as max_evapotranspiration,
    datetime(max(data.max_evapotranspiration_ts), 'unixepoch') as max_evapotranspiration_ts
from (
  select dr.station_id,
          dr.max_gust_wind_speed,
          case when s.gust_wind_speed = dr.max_gust_wind_speed then s.time_stamp else NULL end as max_gust_wind_speed_ts,
          dr.max_average_wind_speed,
          case when s.average_wind_speed = dr.max_average_wind_speed then s.time_stamp else NULL end as max_average_wind_speed_ts,
          dr.max_absolute_pressure,
          case when s.absolute_pressure = dr.max_absolute_pressure then s.time_stamp else NULL end as max_absolute_pressure_ts,
          dr.min_absolute_pressure,
          case when s.absolute_pressure = dr.min_absolute_pressure then s.time_stamp else NULL end as min_absolute_pressure_ts,
          dr.max_apparent_temperature,
          case when s.apparent_temperature = dr.max_apparent_temperature then s.time_stamp else NULL end as max_apparent_temperature_ts,
          dr.min_apparent_temperature,
          case when s.apparent_temperature = dr.min_apparent_temperature then s.time_stamp else NULL end as min_apparent_temperature_ts,
          dr.max_wind_chill,
          case when s.wind_chill = dr.max_wind_chill then s.time_stamp else NULL end as max_wind_chill_ts,
          dr.min_wind_chill,
          case when s.wind_chill = dr.min_wind_chill then s.time_stamp else NULL end as min_wind_chill_ts,
          dr.max_dew_point,
          case when s.dew_point = dr.max_dew_point then s.time_stamp else NULL end as max_dew_point_ts,
          dr.min_dew_point,
          case when s.dew_point = dr.min_dew_point then s.time_stamp else NULL end as min_dew_point_ts,
          dr.max_temperature,
          case when coalesce(ds.high_temperature, s.temperature) = dr.max_temperature then s.time_stamp else NULL end as max_temperature_ts,
          dr.min_temperature,
          case when coalesce(ds.low_temperature, s.temperature) = dr.min_temperature then s.time_stamp else NULL end as min_temperature_ts,
          dr.max_humidity,
          case when s.relative_humidity = dr.max_humidity then s.time_stamp else NULL end as max_humidity_ts,
          dr.min_humidity,
          case when s.relative_humidity = dr.min_humidity then s.time_stamp else NULL end as min_humidity_ts,
          dr.max_solar_radiation,
          case when ds.high_solar_radiation = dr.max_solar_radiation then s.time_stamp else NULL end as max_solar_radiation_ts,
          dr.max_uv_index,
          case when ds.high_uv_index = dr.max_uv_index then s.time_stamp else NULL end as max_uv_index_ts,
          dr.max_rain_rate,
          case when ds.high_rain_rate = dr.max_rain_rate then s.time_stamp else NULL end as max_rain_rate_ts,
          dr.max_evapotranspiration,
          case when ds.evapotranspiration = dr.max_evapotranspiration then s.time_stamp else NULL end as max_evapotranspiration_ts,
          dr.total_rainfall,
          dr.rows_processed
  from sample s
  inner join
  (
            select s.station_id,
                    sum(coalesce(s.rainfall,0)) as "total_rainfall",
                    count(*) as "rows_processed",
                    max(s.gust_wind_speed) as "max_gust_wind_speed",
                    max(s.average_wind_speed) as "max_average_wind_speed",
                    min(s.absolute_pressure) as "min_absolute_pressure",
                    max(s.absolute_pressure) as "max_absolute_pressure",
                    min(s.apparent_temperature) as "min_apparent_temperature",
                    max(s.apparent_temperature) as "max_apparent_temperature",
                    min(s.wind_chill) as "min_wind_chill",
                    max(s.wind_chill) as "max_wind_chill",
                    min(s.dew_point) as "min_dew_point",
                    max(s.dew_point) as "max_dew_point",
                    min(coalesce(ds.low_temperature, s.temperature)) as "min_temperature",
                    max(coalesce(ds.high_temperature, s.temperature)) as "max_temperature",
                    min(s.relative_humidity) as "min_humidity",
                    max(s.relative_humidity) as "max_humidity",
                    max(ds.high_solar_radiation) as "max_solar_radiation", 
                    max(ds.high_uv_index) as "max_uv_index",               
                    max(ds.high_rain_rate) as "max_rain_rate",
                    max(ds.evapotranspiration) as "max_evapotranspiration"
    from sample s
    left outer join davis_sample ds on ds.sample_id = s.sample_id
    group by s.station_id) as dr
  on s.station_id = dr.station_id
  left outer join davis_sample ds on ds.sample_id = s.sample_id
where ( s.gust_wind_speed = dr.max_gust_wind_speed
        or s.average_wind_speed = dr.max_average_wind_speed
        or s.absolute_pressure in (dr.max_absolute_pressure, dr.min_absolute_pressure)
        or s.apparent_temperature in (dr.max_apparent_temperature, dr.min_apparent_temperature)
        or s.wind_chill in (dr.max_wind_chill, dr.min_wind_chill)
        or s.dew_point in (dr.max_dew_point, dr.min_dew_point)
        or coalesce(ds.low_temperature, s.temperature) = dr.min_temperature
        or coalesce(ds.high_temperature, s.temperature) = dr.max_temperature
        or s.relative_humidity in (dr.max_humidity, dr.min_humidity)
        or ds.high_solar_radiation = dr.max_solar_radiation
        or ds.high_uv_index = dr.max_uv_index
        or ds.high_rain_rate = dr.max_rain_rate
        or ds.evapotranspiration = dr.max_evapotranspiration
        )
      ) as data
inner join station stn on stn.station_id = data.station_id
where lower(stn.code) = lower(:stationCode)
group by data.station_id,
        data.max_gust_wind_speed,
        data.max_average_wind_speed,
        data.max_absolute_pressure,
        data.min_absolute_pressure,
        data.min_apparent_temperature,
        data.max_apparent_temperature,
        data.max_wind_chill,
        data.min_wind_chill,
        data.max_dew_point,
        data.min_dew_point,
        data.max_temperature,
        data.min_temperature,
        data.max_humidity,
        data.min_humidity,
        data.total_rainfall,
        data.max_solar_radiation,
        data.max_uv_index,
        data.max_rain_rate,
        data.max_evapotranspiration,
        data.rows_processed
