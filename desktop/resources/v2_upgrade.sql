--begin transaction;

-- //////////////////// Station Table //////////////////// --
drop index station_url;
alter table station rename to station_old;

CREATE TABLE station (
  station_id integer not null primary key,
  code text not null,
  title text,
  description text,
  station_type_id integer,
  sample_interval integer,
  latitude real,
  longitude real,
  altitude real,
  solar_available boolean,
  davis_broadcast_id integer
);
create index station_code on station(code);

insert into station(station_id, code)
    select id, url from station_old;

-- //////////////////// Sample Table //////////////////// --
drop index sample_stn_ts;

alter table sample rename to sample_old;

-- All columns that overlap with the postgres schema should have the same names.
CREATE TABLE sample (
  -- Standard sample columns
  sample_id integer not null primary key,
  -- download_timestamp
  time_stamp integer not null,
  indoor_relative_humidity integer,
  indoor_temperature real,
  relative_humidity integer,
  temperature real,
  dew_point real,
  wind_chill real,
  apparent_temperature real,
  absolute_pressure real,
  average_wind_speed real,
  gust_wind_speed real,
  wind_direction integer,
  rainfall real,
  station_id integer not null,

   -- Davis-specific columns. These live in the davis_sample table in the postgres schema
  solar_radiation real,
  uv_index real,
  reception real,
  high_temperature real,
  low_temperature real,
  high_rain_rate real,
  gust_wind_direction integer,
  evapotranspiration real,
  high_solar_radiation real,
  high_uv_index real,
  forecast_rule_id integer,

  -- sample cache DB only. For tracking where the record came from.
  data_file integer not null
);

create index sample_stn_ts on sample(station_id, time_stamp asc);

insert into sample(sample_id, time_stamp, indoor_relative_humidity, indoor_temperature, relative_humidity,
                   temperature, dew_point, wind_chill, apparent_temperature, absolute_pressure, average_wind_speed,
                   gust_wind_speed, wind_direction, rainfall, station_id, solar_radiation, uv_index, reception,
                   high_temperature, low_temperature, high_rain_rate, gust_wind_direction, evapotranspiration,
                   high_solar_radiation, high_uv_index, forecast_rule_id, data_file)
select id, timestamp, indoor_humidity, indoor_temperature, humidity, temperature, dew_point, wind_chill,
  apparent_temperature, pressure, average_wind_speed, gust_wind_speed, wind_direction, rainfall, station,
  solar_radiation, uv_index, reception, high_temperature, low_temperature, high_rain_rate, gust_wind_direction,
  evapotranspiration, high_solar_radiation, high_uv_index, forecast_rule_id, data_file
from sample_old;

-- //////////////////// Station Type Table //////////////////// --
create table station_type (
  station_type_id integer,
  code text,
  title text
);

INSERT INTO station_type (station_type_id, code, title) VALUES (1, 'FOWH1080', 'Fine Offset WH1080-compatible');
INSERT INTO station_type (station_type_id, code, title) VALUES (2, 'GENERIC', 'Unknown/Generic weather station');
INSERT INTO station_type (station_type_id, code, title) VALUES (3, 'DAVIS', 'Davis Vantage Pro2 or Vantage Vue');


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
    round(((s.reception / 100) * (stn.sample_interval * 1.0) / (((41 + stn.davis_broadcast_id -1) * 1.0) / 16.0 )) * 1.0, 0) as wind_sample_count,
    s.gust_wind_direction,
    s.uv_index as average_uv_index,
    s.evapotranspiration,
    s.high_solar_radiation,
    s.high_uv_index,
    s.forecast_rule_id

  from sample s
    inner join station stn on stn.station_id = s.station_id
;

-- ///////////////////// Indexes /////////////////////
create index idx_rain_time_station on sample(station_id, time_stamp asc, rainfall asc);

-- //////////////////// Update database version //////////////////// --
update db_metadata set v = 2 where k = 'v';
