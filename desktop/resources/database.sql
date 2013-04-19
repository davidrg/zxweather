-- Cache schema - zxweather desktop v1.0

create table station (
  id integer not null primary key,
  url text not null
);

create index station_url on station(url);

-- Cached samples
create table sample (
  id integer not null primary key,
  station integer not null,
  timestamp integer not null,
  temperature real,
  dew_point real,
  apparent_temperature real,
  wind_chill real,
  humidity integer,
  absolute_pressure real,
  indoor_temperature real,
  indoor_humidity integer,
  rainfall real
);

create index sample_stn_ts on sample(station, timestamp asc);

-- The data files we already have cached locally. 
create table cache_entries (
  station integer not null,
  date integer not null,
  primary key (station, date)
);
