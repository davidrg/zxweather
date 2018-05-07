-- Cache schema - zxweather desktop v1.0

-- WARNING: This file is split into multiple statements on the semi-colon
--          character. DO NOT include it in any comments, strings or anywhere
--          else that isn't the end of a statement.

-- WARNING: Comments are manually stripped out of this file. For them to be
--          correctly stripped out ensure they always begin at the start of the
--          line.

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

-- For compatibility with the postgres schema
create table station_type (
  station_type_id integer,
  code text,
  title text
);

INSERT INTO station_type (station_type_id, code, title) VALUES (1, 'FOWH1080', 'Fine Offset WH1080-compatible');
INSERT INTO station_type (station_type_id, code, title) VALUES (2, 'GENERIC', 'Unknown/Generic weather station');
INSERT INTO station_type (station_type_id, code, title) VALUES (3, 'DAVIS', 'Davis Vantage Pro2 or Vantage Vue');

-- Information on the specific data files that cached information has been
-- pulled from.
create table data_file (
  id integer not null primary key,
  station integer not null,
  url text not null,
  last_modified integer not null,
  size integer not null
);

create index file_url on data_file(url);

-- Cached samples
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
  reception real, -- the davis_sample view turns this back into wind sample count with the help of the data in the station table
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

create index sample_stn_ts on sample(station, time_stamp asc);

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

-- Information about cameras, etc
create table image_source (
  id integer not null primary key,
  station integer not null,
  code text not null,
  name text not null,
  description text
);

create index image_source_code_and_station on image_source(station, code);

-- This is the image sources equivalent of the data_file table.
create table image_set (
    id integer not null primary key,
    image_source integer not null,
    url text not null,
    last_modified integer not null,
    size integer not null,
    is_valid integer not null -- 0 = invalid, 1 = valid
);

create index image_set_url on image_set(url);

-- An image (or other media) captured by an image source
-- Note that IDs in this table are only unique for a particular image source.
create table image (
  id integer not null,
  image_set integer not null,
  source integer not null,
  timestamp integer not null,
  date text not null,
  type_code text not null,
  type_name text,
  title text,
  description text,
  mime_type text,
  url text,
  metadata text,
  meta_url text
);

create index image_id on image(id, source);
create index image_date on image(date);

-- Database metadata - version number, etc.
create table db_metadata (
  k text not null primary key,
  v text
);

insert into db_metadata(k,v) values('v','2');

-- Try to enable the write ahead log (requires SQLite 3.7.0+)
-- This improves performance by quite a bit.
PRAGMA journal_mode=WAL;
