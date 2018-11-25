drop view davis_sample;

-- Target database: V2
-- Upgrades to: V3
--
-- Fixes a bug in the wind_sample_count column of the davis_sample view.

-- //////////////////// Davis Sample View //////////////////// --
-- View to emulate the davis_sample table in the postgres schema
create view davis_sample as
  select
    s.sample_id,
    -- record_time
    -- record_date
    s.high_temperature,
    s.low_temperature,
    s.high_rain_rate,
    s.solar_radiation,
    -- convert reception back into wind samples
    cast(round(((s.reception / 100) * (stn.sample_interval*60 * 1.0) / (((41 + stn.davis_broadcast_id -1) * 1.0) / 16.0 )) * 1.0, 0) as int) as wind_sample_count,
    s.gust_wind_direction,
    s.uv_index as average_uv_index,
    s.evapotranspiration,
    s.high_solar_radiation,
    s.high_uv_index,
    s.forecast_rule_id

  from sample s
    inner join station stn on stn.station_id = s.station_id
;

-- //////////////////// Update database version //////////////////// --
update db_metadata set v = 3 where k = 'v';
