-- Cache schema - zxweather desktop v1.0

-- WARNING: This file is split into multiple statements on the semi-colon
--          character. DO NOT include it in any comments, strings or anywhere
--          else that isn't the end of a statement.

-- WARNING: Comments are manually stripped out of this file. For them to be
--          correctly stripped out ensure they always begin at the start of the
--          line.

create table station (
  id integer not null primary key,
  url text not null
);

create index station_url on station(url);

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
create table sample (
  id integer not null primary key,
  station integer not null,
  data_file integer not null,
  timestamp integer not null,
  temperature real,
  dew_point real,
  apparent_temperature real,
  wind_chill real,
  humidity integer,
  pressure real,
  indoor_temperature real,
  indoor_humidity integer,
  rainfall real,
  average_wind_speed real,
  gust_wind_speed real,
  wind_direction integer,
  solar_radiation real,
  uv_index real
);

create index sample_stn_ts on sample(station, timestamp asc);

-- Information about cameras, etc
create table image_source (
  station integer not null,
  code text not null,
  name text not null,
  description text
);

-- An image (or other media) captured by an image source
create table image (
  id integer not null,
  source integer not null,
  timestamp integer not null,
  date text not null,
  type_code text not null,
  title text,
  description text,
  mime_type text
);

create index image_id on image(id);
create index image_date on image(date);

-- Database metadata - version number, etc.
create table db_metadata (
  k text not null primary key,
  v text
);

insert into db_metadata(k,v) values('v','1');

-- Try to enable the write ahead log (requires SQLite 3.7.0+)
-- This improves performance by quite a bit.
PRAGMA journal_mode=WAL;
