alter table sample add column leaf_wetness_1 real;
alter table sample add column leaf_wetness_2 real;
alter table sample add column leaf_temperature_1 real;
alter table sample add column leaf_temperature_2 real;
alter table sample add column soil_moisture_1 real;
alter table sample add column soil_moisture_2 real;
alter table sample add column soil_moisture_3 real;
alter table sample add column soil_moisture_4 real;
alter table sample add column soil_temperature_1 real;
alter table sample add column soil_temperature_2 real;
alter table sample add column soil_temperature_3 real;
alter table sample add column soil_temperature_4 real;
alter table sample add column extra_humidity_1 real;
alter table sample add column extra_humidity_2 real;
alter table sample add column extra_temperature_1 real;
alter table sample add column extra_temperature_2 real;
alter table sample add column extra_temperature_3 real;

DROP VIEW IF EXISTS davis_sample;

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
    s.forecast_rule_id,
    s.leaf_wetness_1,
    s.leaf_wetness_2,
    s.leaf_temperature_1,
    s.leaf_temperature_2,
    s.soil_moisture_1,
    s.soil_moisture_2,
    s.soil_moisture_3,
    s.soil_moisture_4,
    s.soil_temperature_1,
    s.soil_temperature_2,
    s.soil_temperature_3,
    s.soil_temperature_4,
    s.extra_humidity_1,
    s.extra_humidity_2,
    s.extra_temperature_1,
    s.extra_temperature_2,
    s.extra_temperature_3
  from sample s
    inner join station stn on stn.station_id = s.station_id
;

update db_metadata set v = 5 where k = 'v';