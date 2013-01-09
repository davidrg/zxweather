----------------------------------------------------------------------------
-- This script is to upgrade from the v0.1 schema.                        --
----------------------------------------------------------------------------

-- If you are running this script manually you should uncomment the following
-- BEGIN statement and the matching COMMIT statement at the end of the file.

--BEGIN;

----------------------------------------------------------------------
-- REMOVE OLD STRUCTURE ----------------------------------------------
----------------------------------------------------------------------

-- Firstly, move the sample table so we can create the new one.
ALTER TABLE sample RENAME TO samples_v1;
alter sequence sample_sample_id_seq rename to sample_v1_sample_id_seq;
alter index pk_sample rename to pk_sample_v1;

-- The column spec for these views changes (to include the station id) so
-- we can't just replace them.
drop view latest_record_number; -- This also gets renamed.
drop view daily_records;
drop view monthly_records;
drop view yearly_records;

-- Easier to just drop and recreate this as it doesn't contain anything
-- important.
drop table live_data;

-- More stuff we can just recreate
drop index idx_time_stamp;

----------------------------------------------------------------------
-- DOMAINS -----------------------------------------------------------
----------------------------------------------------------------------

-- Fix up the rh_percentage domain. This may have previously been done
-- by the v0.1.2 hotfix. The check constraint being deleted is now
-- on the sample table.
ALTER DOMAIN rh_percentage
DROP NOT NULL;

alter domain rh_percentage drop constraint if exists rh_percentage_check;

----------------------------------------------------------------------
-- TABLES ------------------------------------------------------------
----------------------------------------------------------------------

CREATE TABLE station_type
(
  station_type_id serial NOT NULL,
  code varchar(8) not null,
  title character varying,
  CONSTRAINT pk_station_type PRIMARY KEY (station_type_id)
);

COMMENT ON TABLE station_type IS 'A station hardware type (eg, Fine Offset WH1080-compatible).';
COMMENT ON COLUMN station_type.code IS 'A short code for the station type';
COMMENT ON COLUMN station_type.title IS 'A user-readable title for the station type';

-- Only one station type supported at the moment.
INSERT INTO station_type(code, title) VALUES('FOWH1080', 'Fine Offset WH1080-compatible');
INSERT INTO station_type(code, title) VALUES('GENERIC', 'Unknown/Generic weather station');

-- Information on stations in this database.
CREATE TABLE station
(
  station_id serial NOT NULL,
  code varchar(5) NOT NULL UNIQUE,
  title character varying NOT NULL,
  description character varying,
  station_type_id INTEGER NOT NULL REFERENCES station_type(station_type_id),
  sample_interval integer not null,
  live_data_available boolean not null default true,
  CONSTRAINT pk_station PRIMARY KEY (station_id)
);

COMMENT ON TABLE station is 'Information about a weather station in this database';
COMMENT ON COLUMN station.code is 'A short code for the station. Used, for example, in the web interfaces URLs';
COMMENT ON COLUMN station.title is 'A user-readable title for the station';
COMMENT ON COLUMN station.description is 'A user-readable description for the station';
COMMENT ON COLUMN station.station_type_id is 'The type of hardware this station uses.';
COMMENT ON COLUMN station.sample_interval is 'How often (in seconds) new samples are logged.';
COMMENT ON COLUMN station.live_data_available is 'If live data is available from the live_data table for this station.';

-- Generic sample data. Anything that is specific to a particular station type
-- is in that station-specific table.
CREATE TABLE sample
(
  sample_id serial NOT NULL,
  download_timestamp timestamp with time zone, -- When this record was downloaded from the weather station
  time_stamp timestamp with time zone, -- The calculated date and time when this record was likely recorded by the weather station. How far out it could possibly be depends on the size of the sample interval, etc.
  indoor_relative_humidity rh_percentage, -- Relative Humidity at the base station
  indoor_temperature real, -- Temperature at the base station
  relative_humidity rh_percentage, -- Relative Humidity
  temperature real, -- Temperature outside
  dew_point real, -- Calculated dew point
  wind_chill real, -- Calculated wind chill
  apparent_temperature real, -- Calculated apparent temperature.
  absolute_pressure real, -- Absolute pressure
  average_wind_speed real, -- Average Wind Speed.
  gust_wind_speed real, -- Gust wind speed.
  wind_direction wind_direction, -- Wind Direction.
  rainfall real, -- Calculated rainfall. Calculation is based on total_rainfall and rain_overflow columns compared to the previous sample.
  station_id integer not null references station(station_id),
  CONSTRAINT pk_sample PRIMARY KEY (sample_id),
  CONSTRAINT chk_indoor_relative_humidity CHECK (indoor_relative_humidity::integer > 0 AND indoor_relative_humidity::integer < 100), -- Ensure the indoor relative humidity is in the range 0-100
  CONSTRAINT chk_outdoor_relative_humidity CHECK (relative_humidity::integer > 0 AND relative_humidity::integer < 100) -- Ensure the outdoor relative humidity is in the range 0-100
);
ALTER TABLE sample
  OWNER TO postgres;
COMMENT ON TABLE sample IS 'Samples from the weather station.';
COMMENT ON COLUMN sample.download_timestamp IS 'When this record was downloaded from the weather station';
COMMENT ON COLUMN sample.time_stamp IS 'The calculated date and time when this record was likely recorded by the weather station. How far out it could possibly be depends on the size of the sample interval, etc.';
COMMENT ON COLUMN sample.indoor_relative_humidity IS 'Relative Humidity at the base station. 0-99%';
COMMENT ON COLUMN sample.indoor_temperature IS 'Temperature at the base station in degrees Celsius';
COMMENT ON COLUMN sample.relative_humidity IS 'Relative Humidity. 0-99%';
COMMENT ON COLUMN sample.temperature IS 'Temperature outside in degrees Celsius';
COMMENT ON COLUMN sample.dew_point IS 'Calculated dew point in degrees Celsius. Use the dew_point function to calculate. Calculated automatically by a trigger on insert.';
COMMENT ON COLUMN sample.wind_chill IS 'Calculated wind chill in degrees Celsius. Use the wind_chill function to calculate. Calculated automatically by a trigger on insert.';
COMMENT ON COLUMN sample.apparent_temperature IS 'Calculated apparent temperature in degrees Celsius. Use the apparent_temperature to calculate. Calculated automatically by a trigger on insert.';
COMMENT ON COLUMN sample.absolute_pressure IS 'Absolute pressure in hPa';
COMMENT ON COLUMN sample.average_wind_speed IS 'Average Wind Speed in m/s.';
COMMENT ON COLUMN sample.gust_wind_speed IS 'Gust wind speed in m/s.';
COMMENT ON COLUMN sample.wind_direction IS 'Wind Direction.';
COMMENT ON COLUMN sample.rainfall IS 'Calculated rainfall in mm. Smallest recordable amount is 0.3mm. Calculation is based on total_rainfall and rain_overflow columns compared to the previous sample.';
COMMENT ON COLUMN sample.station_id IS 'The weather station this sample is for';

COMMENT ON CONSTRAINT chk_indoor_relative_humidity ON sample IS 'Ensure the indoor relative humidity is in the range 0-100';
COMMENT ON CONSTRAINT chk_outdoor_relative_humidity ON sample IS 'Ensure the outdoor relative humidity is in the range 0-100';

-- A table for data specific to WH1080-compatible hardware.
CREATE TABLE wh1080_sample
(
  sample_id integer not null primary key references sample(sample_id),
  sample_interval integer, -- Number of minutes since the previous sample
  record_number integer NOT NULL, -- The history slot in the weather stations circular buffer this record came from. Used for calculating the timestamp.
  last_in_batch boolean, -- If this was the last record in a batch of records downloaded from the weather station.
  invalid_data boolean NOT NULL, -- If the record from the weather stations "no sensor data received" flag is set.
  total_rain real, -- Total rain recorded by the sensor so far. Subtract the previous samples total rainfall from this one to calculate the amount of rain for this sample.
  rain_overflow boolean -- If an overflow in the total_rain counter has occurred
);

COMMENT ON TABLE wh1080_sample IS 'Data for samples taken from WH1080-compatible hardware';
COMMENT ON COLUMN wh1080_sample.sample_interval IS 'Number of minutes since the previous sample';
COMMENT ON COLUMN wh1080_sample.record_number IS 'The history slot in the weather stations circular buffer this record came from. Used for calculating the timestamp.';
COMMENT ON COLUMN wh1080_sample.last_in_batch IS 'If this was the last record in a batch of records downloaded from the weather station.';
COMMENT ON COLUMN wh1080_sample.invalid_data IS 'If the record from the weather stations "no sensor data received" flag is set.';
COMMENT ON COLUMN wh1080_sample.total_rain IS 'Total rain recorded by the sensor so far. Smallest possible increment is 0.3mm. Subtract the previous samples total rainfall from this one to calculate the amount of rain for this sample.';
COMMENT ON COLUMN wh1080_sample.rain_overflow IS 'If an overflow in the total_rain counter has occurred';

----------------------
CREATE TABLE live_data
(
  station_id integer not null primary key references station(station_id),
  download_timestamp timestamp with time zone, -- When this record was downloaded from the weather station
  indoor_relative_humidity integer, -- Relative Humidity at the base station
  indoor_temperature real, -- Temperature at the base station
  relative_humidity integer, -- Relative Humidity
  temperature real, -- Temperature outside
  dew_point real, -- Calculated dew point
  wind_chill real, -- Calculated wind chill
  apparent_temperature real, -- Calculated apparent temperature.
  absolute_pressure real, -- Absolute pressure
    average_wind_speed real, -- Average Wind Speed.
  gust_wind_speed real, -- Gust wind speed.
  wind_direction wind_direction -- Wind Direction.
);
ALTER TABLE live_data
  OWNER TO postgres;
COMMENT ON TABLE live_data
IS 'Live data from the weather stations. Contains only a single record for each weather station.';
COMMENT ON COLUMN live_data.download_timestamp IS 'When this record was downloaded from the weather station';
COMMENT ON COLUMN live_data.indoor_relative_humidity IS 'Relative Humidity at the base station. 0-99%';
COMMENT ON COLUMN live_data.indoor_temperature IS 'Temperature at the base station in degrees Celsius';
COMMENT ON COLUMN live_data.relative_humidity IS 'Relative Humidity. 0-99%';
COMMENT ON COLUMN live_data.temperature IS 'Temperature outside in degrees Celsius';
COMMENT ON COLUMN live_data.dew_point IS 'Calculated dew point in degrees Celsius. Use the dew_point function to calculate. Calculated automatically by a trigger on insert.';
COMMENT ON COLUMN live_data.wind_chill IS 'Calculated wind chill in degrees Celsius. Use the wind_chill function to calculate. Calculated automatically by a trigger on insert.';
COMMENT ON COLUMN live_data.apparent_temperature IS 'Calculated apparent temperature in degrees Celsius. Use the apparent_temperature to calculate. Calculated automatically by a trigger on insert.';
COMMENT ON COLUMN live_data.absolute_pressure IS 'Absolute pressure in hPa';
COMMENT ON COLUMN live_data.average_wind_speed IS 'Average Wind Speed in m/s.';
COMMENT ON COLUMN live_data.gust_wind_speed IS 'Gust wind speed in m/s.';
COMMENT ON COLUMN live_data.wind_direction IS 'Wind Direction.';
COMMENT ON COLUMN live_data.station_id is 'The station this live data is for.';

-- Live data table must always have one and only one record.
--insert into live_data(download_timestamp, wind_direction) values(null, 'INV');


-- A table to store some basic information about the database (such as schema
-- version).
CREATE TABLE db_info
(
  k varchar(20) primary key not null,
  v character varying
);

COMMENT ON TABLE db_info IS 'Basic database info (key/value data)';
COMMENT ON COLUMN db_info.k is 'Data key';
COMMENT ON COLUMN db_info.v is 'Data value';

-- This is schema revision 2
insert into db_info(k,v) VALUES('DB_VERSION','2');

-- And it its not compatible with anything older than zxweather 0.2.0
insert into db_info(k,v) VALUES('MIN_VER_MAJ', '0');
insert into db_info(k,v) VALUES('MIN_VER_MIN', '2');
insert into db_info(k,v) VALUES('MIN_VER_REV', '0');

----------------------------------------------------------------------
-- INDICIES ----------------------------------------------------------
----------------------------------------------------------------------

-- Cuts recalculating rainfall for 3000 records from 4200ms to 110ms.
CREATE INDEX idx_time_stamp
ON sample (time_stamp ASC NULLS LAST);
COMMENT ON INDEX idx_time_stamp
IS 'To make operations such as recalculating rainfall a little quicker.';

----------------------------------------------------------------------
-- VIEWS -------------------------------------------------------------
----------------------------------------------------------------------

CREATE OR REPLACE VIEW wh1080_latest_record_number AS
          SELECT ws.record_number, s.time_stamp, s.station_id, st.code
  from sample s
          inner join wh1080_sample ws on ws.sample_id = s.sample_id
          inner join station st on st.station_id = s.station_id
  order by s.time_stamp desc, ws.record_number desc
  limit 1;

ALTER TABLE wh1080_latest_record_number
  OWNER TO postgres;
COMMENT ON VIEW wh1080_latest_record_number
IS 'Gets the number of the most recent record for WH1080-type stations. This is used by wh1080d and associated utilities to figure out what it needs to load into the database and what is already there.';


CREATE OR REPLACE VIEW daily_records AS
          select data.date_stamp,
                  data.station_id,
                  data.total_rainfall,
                  data.max_gust_wind_speed,
                  max(data.max_gust_wind_speed_ts) as max_gust_wind_speed_ts,
                  data.max_average_wind_speed,
                  max(data.max_average_wind_speed_ts) as max_average_wind_speed_ts,
                  data.min_absolute_pressure,
                  max(data.min_absolute_pressure_ts) as min_absolute_pressure_ts,
                  data.max_absolute_pressure,
                  max(data.max_absolute_pressure_ts) as max_absolute_pressure_ts,
                  data.min_apparent_temperature,
                  max(data.min_apparent_temperature_ts) as min_apparent_temperature_ts,
                  data.max_apparent_temperature,
                  max(data.max_apparent_temperature_ts) as max_apparent_temperature_ts,
                  data.min_wind_chill,
                  max(data.min_wind_chill_ts) as min_wind_chill_ts,
                  data.max_wind_chill,
                  max(data.max_wind_chill_ts) as max_wind_chill_ts,
                  data.min_dew_point,
                  max(data.min_dew_point_ts) as min_dew_point_ts,
                  data.max_dew_point,
                  max(data.max_dew_point_ts) as max_dew_point_ts,
                  data.min_temperature,
                  max(data.min_temperature_ts) as min_temperature_ts,
                  data.max_temperature,
                  max(data.max_temperature_ts) as max_temperature_ts,
                  data.min_humidity,
                  max(data.min_humidity_ts) as min_humidity_ts,
                  data.max_humidity,
                  max(data.max_humidity_ts) as max_humidity_ts
  from (
    select dr.date_stamp,
           dr.station_id,
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
            case when s.temperature = dr.max_temperature then s.time_stamp else NULL end as max_temperature_ts,
            dr.min_temperature,
            case when s.temperature = dr.min_temperature then s.time_stamp else NULL end as min_temperature_ts,
            dr.max_humidity,
            case when s.relative_humidity = dr.max_humidity then s.time_stamp else NULL end as max_humidity_ts,
            dr.min_humidity,
            case when s.relative_humidity = dr.min_humidity then s.time_stamp else NULL end as min_humidity_ts,
            dr.total_rainfall
    from sample s
            inner join
            (
              select date(time_stamp) as date_stamp,
                      s.station_id,
                      sum(coalesce(s.rainfall,0)) as "total_rainfall",
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
                      min(s.temperature) as "min_temperature",
                      max(s.temperature) as "max_temperature",
                      min(s.relative_humidity) as "min_humidity",
                      max(s.relative_humidity) as "max_humidity"
              from sample s
              group by date_stamp, s.station_id) as dr
      on date(s.time_stamp) = dr.date_stamp

          -- Filter the samples down to only those with a maximum or minimum reading
    where ( s.gust_wind_speed = dr.max_gust_wind_speed
            or s.average_wind_speed = dr.max_average_wind_speed
            or s.absolute_pressure in (dr.max_absolute_pressure, dr.min_absolute_pressure)
            or s.apparent_temperature in (dr.max_apparent_temperature, dr.min_apparent_temperature)
            or s.wind_chill in (dr.max_wind_chill, dr.min_wind_chill)
            or s.dew_point in (dr.max_dew_point, dr.min_dew_point)
            or s.temperature in (dr.max_temperature, dr.min_temperature)
            or s.relative_humidity in (dr.max_humidity, dr.min_humidity)
            )
          ) as data
  group by data.date_stamp,
          data.station_id,
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
          data.total_rainfall
  order by data.date_stamp desc;
COMMENT ON VIEW daily_records
IS 'Minimum and maximum temperature, dew point, wind chill, apparent temperature, gust wind speed, average wind speed, absolute pressure and humidity per day.';



CREATE OR REPLACE VIEW monthly_records AS
          select data.date_stamp,
                  data.station_id,
                  data.total_rainfall,
                  data.max_gust_wind_speed,
                  max(data.max_gust_wind_speed_ts) as max_gust_wind_speed_ts,
                  data.max_average_wind_speed,
                  max(data.max_average_wind_speed_ts) as max_average_wind_speed_ts,
                  data.min_absolute_pressure,
                  max(data.min_absolute_pressure_ts) as min_absolute_pressure_ts,
                  data.max_absolute_pressure,
                  max(data.max_absolute_pressure_ts) as max_absolute_pressure_ts,
                  data.min_apparent_temperature,
                  max(data.min_apparent_temperature_ts) as min_apparent_temperature_ts,
                  data.max_apparent_temperature,
                  max(data.max_apparent_temperature_ts) as max_apparent_temperature_ts,
                  data.min_wind_chill,
                  max(data.min_wind_chill_ts) as min_wind_chill_ts,
                  data.max_wind_chill,
                  max(data.max_wind_chill_ts) as max_wind_chill_ts,
                  data.min_dew_point,
                  max(data.min_dew_point_ts) as min_dew_point_ts,
                  data.max_dew_point,
                  max(data.max_dew_point_ts) as max_dew_point_ts,
                  data.min_temperature,
                  max(data.min_temperature_ts) as min_temperature_ts,
                  data.max_temperature,
                  max(data.max_temperature_ts) as max_temperature_ts,
                  data.min_humidity,
                  max(data.min_humidity_ts) as min_humidity_ts,
                  data.max_humidity,
                  max(data.max_humidity_ts) as max_humidity_ts
  from (
            select dr.date_stamp,
                    dr.station_id,
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
                    case when s.temperature = dr.max_temperature then s.time_stamp else NULL end as max_temperature_ts,
                    dr.min_temperature,
                    case when s.temperature = dr.min_temperature then s.time_stamp else NULL end as min_temperature_ts,
                    dr.max_humidity,
                    case when s.relative_humidity = dr.max_humidity then s.time_stamp else NULL end as max_humidity_ts,
                    dr.min_humidity,
                    case when s.relative_humidity = dr.min_humidity then s.time_stamp else NULL end as min_humidity_ts,
                    dr.total_rainfall
    from sample s
            inner join
            (
                      select date(date_trunc('month', s.time_stamp)) as date_stamp,
                              s.station_id,
                              sum(coalesce(s.rainfall,0)) as "total_rainfall",
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
                              min(s.temperature) as "min_temperature",
                              max(s.temperature) as "max_temperature",
                              min(s.relative_humidity) as "min_humidity",
                              max(s.relative_humidity) as "max_humidity"
              from sample s
              group by date_stamp, s.station_id) as dr
      on date(date_trunc('month', s.time_stamp)) = dr.date_stamp

          -- Filter the samples down to only those with a maximum or minimum reading
    where ( s.gust_wind_speed = dr.max_gust_wind_speed
            or s.average_wind_speed = dr.max_average_wind_speed
            or s.absolute_pressure in (dr.max_absolute_pressure, dr.min_absolute_pressure)
            or s.apparent_temperature in (dr.max_apparent_temperature, dr.min_apparent_temperature)
            or s.wind_chill in (dr.max_wind_chill, dr.min_wind_chill)
            or s.dew_point in (dr.max_dew_point, dr.min_dew_point)
            or s.temperature in (dr.max_temperature, dr.min_temperature)
            or s.relative_humidity in (dr.max_humidity, dr.min_humidity)
            )
          ) as data
  group by data.date_stamp,
          data.station_id,
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
          data.total_rainfall
  order by data.date_stamp desc;
COMMENT ON VIEW monthly_records
IS 'Minimum and maximum values for each month.';


CREATE OR REPLACE VIEW yearly_records AS
          select data.year_stamp,
                  data.station_id,
                  data.total_rainfall,
                  data.max_gust_wind_speed,
                  max(data.max_gust_wind_speed_ts) as max_gust_wind_speed_ts,
                  data.max_average_wind_speed,
                  max(data.max_average_wind_speed_ts) as max_average_wind_speed_ts,
                  data.min_absolute_pressure,
                  max(data.min_absolute_pressure_ts) as min_absolute_pressure_ts,
                  data.max_absolute_pressure,
                  max(data.max_absolute_pressure_ts) as max_absolute_pressure_ts,
                  data.min_apparent_temperature,
                  max(data.min_apparent_temperature_ts) as min_apparent_temperature_ts,
                  data.max_apparent_temperature,
                  max(data.max_apparent_temperature_ts) as max_apparent_temperature_ts,
                  data.min_wind_chill,
                  max(data.min_wind_chill_ts) as min_wind_chill_ts,
                  data.max_wind_chill,
                  max(data.max_wind_chill_ts) as max_wind_chill_ts,
                  data.min_dew_point,
                  max(data.min_dew_point_ts) as min_dew_point_ts,
                  data.max_dew_point,
                  max(data.max_dew_point_ts) as max_dew_point_ts,
                  data.min_temperature,
                  max(data.min_temperature_ts) as min_temperature_ts,
                  data.max_temperature,
                  max(data.max_temperature_ts) as max_temperature_ts,
                  data.min_humidity,
                  max(data.min_humidity_ts) as min_humidity_ts,
                  data.max_humidity,
                  max(data.max_humidity_ts) as max_humidity_ts
  from (
            select dr.year_stamp,
                    dr.station_id,
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
                    case when s.temperature = dr.max_temperature then s.time_stamp else NULL end as max_temperature_ts,
                    dr.min_temperature,
                    case when s.temperature = dr.min_temperature then s.time_stamp else NULL end as min_temperature_ts,
                    dr.max_humidity,
                    case when s.relative_humidity = dr.max_humidity then s.time_stamp else NULL end as max_humidity_ts,
                    dr.min_humidity,
                    case when s.relative_humidity = dr.min_humidity then s.time_stamp else NULL end as min_humidity_ts,
                    dr.total_rainfall
    from sample s
            inner join
            (
                      select extract(year from s.time_stamp) as year_stamp,
                              s.station_id,
                              sum(coalesce(s.rainfall,0)) as "total_rainfall",
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
                              min(s.temperature) as "min_temperature",
                              max(s.temperature) as "max_temperature",
                              min(s.relative_humidity) as "min_humidity",
                              max(s.relative_humidity) as "max_humidity"
              from sample s
              group by year_stamp, s.station_id) as dr
      on extract(year from s.time_stamp) = dr.year_stamp

          -- Filter the samples down to only those with a maximum or minimum reading
    where ( s.gust_wind_speed = dr.max_gust_wind_speed
            or s.average_wind_speed = dr.max_average_wind_speed
            or s.absolute_pressure in (dr.max_absolute_pressure, dr.min_absolute_pressure)
            or s.apparent_temperature in (dr.max_apparent_temperature, dr.min_apparent_temperature)
            or s.wind_chill in (dr.max_wind_chill, dr.min_wind_chill)
            or s.dew_point in (dr.max_dew_point, dr.min_dew_point)
            or s.temperature in (dr.max_temperature, dr.min_temperature)
            or s.relative_humidity in (dr.max_humidity, dr.min_humidity)
            )
          ) as data
  group by data.year_stamp,
          data.station_id,
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
          data.total_rainfall
  order by data.year_stamp desc;
COMMENT ON VIEW yearly_records
IS 'Minimum and maximum records for each year.';




----------------------------------------------------------------------
-- FUNCTIONS ---------------------------------------------------------
----------------------------------------------------------------------

-- These are unchanged.

----------------------------------------------------------------------
-- TRIGGER FUNCTIONS -------------------------------------------------
----------------------------------------------------------------------

-- Computes any computed fields when a new sample is inserted (dew point, wind chill, aparent temperature, etc)
CREATE OR REPLACE FUNCTION compute_sample_values()
  RETURNS trigger AS
$BODY$
DECLARE
    station_code character varying;
BEGIN
    -- If its an insert, calculate any fields that need calculating. We will ignore updates here.
    IF(TG_OP = 'INSERT') THEN

        -- Various calculated temperatures
        NEW.dew_point = dew_point(NEW.temperature, NEW.relative_humidity);
        NEW.wind_chill = wind_chill(NEW.temperature, NEW.average_wind_speed);
        NEW.apparent_temperature = apparent_temperature(NEW.temperature, NEW.average_wind_speed, NEW.relative_humidity);

        -- Rainfall calculations (if required) are performed on trigger
        -- functions attached to the hardware-specific tables now.

        -- Grab the station code and send out a notification.
        select s.code into station_code
        from station s where s.station_id = NEW.station_id;

        perform pg_notify('new_sample', station_code);
    END IF;

    RETURN NEW;
END;
$BODY$
LANGUAGE plpgsql VOLATILE;
COMMENT ON FUNCTION compute_sample_values() IS 'Calculates values for all calculated fields (wind chill, dew point, rainfall, etc).';

-- Computes any computed fields when a new wh1080 sample is inserted (dew point, wind chill, aparent temperature, etc)
CREATE OR REPLACE FUNCTION compute_wh1080_sample_values()
  RETURNS trigger AS
$BODY$
DECLARE
    new_rainfall real;
    current_station_id integer;
    new_timestamp timestamptz;
BEGIN
    -- If its an insert, calculate any fields that need calculating. We will ignore updates here.
    IF(TG_OP = 'INSERT') THEN

        IF(NEW.invalid_data) THEN
            -- The outdoor sample data in this record is garbage. Discard
            -- it to prevent crazy data.

            update sample
            set relative_humidity = null,
                temperature = null,
                dew_point = null,
                wind_chill = null,
                apparent_temperature = null,
                average_wind_speed = null,
                gust_wind_speed = null,
                rainfall = null
            where sample_id = NEW.sample_id;
            -- Wind direction should be fine (it'll be 'INV').
            -- The rest of the fields are captured by the base station
            -- and so should be fine.

        ELSE
            -- Figure out station ID
            select station_id, time_stamp into current_station_id, new_timestamp
            from sample where sample_id = NEW.sample_id;

            -- Calculate actual rainfall for this record from the total rainfall
            -- accumulator of this record and the previous record.
            -- 19660.8 is the maximum rainfall accumulator value (65536 * 0.3mm).
             select into new_rainfall
                        CASE WHEN NEW.total_rain - prev.total_rain >= 0 THEN
                            NEW.total_rain - prev.total_rain
                        ELSE
                            NEW.total_rain + (19660.8 - prev.total_rain)
                        END as rainfall
            from wh1080_sample prev
            inner join sample sa on sa.sample_id = prev.sample_id
            -- find the previous sample:
            where sa.time_stamp = (select max(time_stamp)
                                from sample ins
                                where ins.time_stamp < new_timestamp)
            and sa.station_id = current_station_id;

            update sample set rainfall = new_rainfall
            where sample_id = NEW.sample_id;
        END IF;
    END IF;
    RETURN NEW;
END;
$BODY$
LANGUAGE plpgsql VOLATILE;
COMMENT ON FUNCTION compute_wh1080_sample_values() IS 'Calculates values for all calculated fields (wind chill, dew point, rainfall, etc).';

-- Calculates dewpoint, wind chill, apparent temperature, etc.
CREATE OR REPLACE FUNCTION live_data_update() RETURNS trigger AS
        $BODY$
DECLARE
  station_code character varying;
BEGIN
    IF(TG_OP = 'UPDATE') THEN
        -- Various calculated temperatures
        NEW.dew_point = dew_point(NEW.temperature, NEW.relative_humidity);
        NEW.wind_chill = wind_chill(NEW.temperature, NEW.average_wind_speed);
        NEW.apparent_temperature = apparent_temperature(NEW.temperature, NEW.average_wind_speed, NEW.relative_humidity);

        -- Grab the station code and send out a notification.
        select s.code into station_code
        from station s where s.station_id = NEW.station_id;

        perform pg_notify('live_data_updated', live_data_updated);

    END IF;

    RETURN NEW;
END;$BODY$
LANGUAGE plpgsql VOLATILE;
COMMENT ON FUNCTION live_data_update() IS 'Calculates values for all calculated fields.';


----------------------------------------------------------------------
-- TRIGGERS ----------------------------------------------------------
----------------------------------------------------------------------

CREATE TRIGGER calculate_fields BEFORE INSERT
ON sample FOR EACH ROW
EXECUTE PROCEDURE public.compute_sample_values();
COMMENT ON TRIGGER calculate_fields ON sample IS 'Calculate any calculated fields.';

CREATE TRIGGER calculate_wh1080_fields BEFORE INSERT
ON wh1080_sample FOR EACH ROW
EXECUTE PROCEDURE public.compute_wh1080_sample_values();
COMMENT ON TRIGGER calculate_wh1080_fields ON wh1080_sample IS 'Calculate fields that need WH1080-specific data.';

CREATE TRIGGER live_data_update BEFORE INSERT OR DELETE OR UPDATE
ON live_data FOR EACH ROW
EXECUTE PROCEDURE public.live_data_update();
COMMENT ON TRIGGER live_data_update ON live_data IS 'Calculates calculated fields for updates, ignores everything else.';

----------------------------------------------------------------------
-- MIGRATE DATA ------------------------------------------------------
----------------------------------------------------------------------


-- This will create a new weather station and migrate all data across from
-- the old v1 table to the new database structure.
DO LANGUAGE plpgsql
$$
DECLARE
  wh1080_id integer;
  new_station_id integer;
  temp_sample_id integer;
  rec record;
BEGIN

  RAISE NOTICE 'Performing data migration...';

  -- Get the ID for the WH1080 station type.
  SELECT station_type_id INTO wh1080_id
  FROM station_type WHERE code = 'FOWH1080';

  -- Create a new entry for the weather station
  INSERT INTO station(code, description, title, station_type_id, sample_interval)
               VALUES('UNKN',
                      'Unknown WH1080-compatible weather station migrated from zxweather v0.1',
                      'Unknown WH1080-compatible Weather Station',
                      wh1080_id,
                      300)
  RETURNING station_id INTO new_station_id;

  -- Create live data record
  INSERT INTO live_data(wind_direction,station_id) VALUES('INV', new_station_id);

  -- Migrate sample data
  FOR rec IN SELECT sample_interval, record_number, download_timestamp,
                       last_in_batch, invalid_Data, time_stamp,
                       indoor_relative_humidity, indoor_temperature,
                       relative_humidity, temperature, absolute_pressure,
                       average_wind_speed, gust_wind_speed, wind_direction,
                       total_rain, rain_overflow
                FROM samples_v1
                ORDER BY time_stamp asc
  LOOP
    INSERT INTO sample(download_timestamp,
                       time_stamp,
                       indoor_relative_humidity,
                       indoor_temperature,
                       relative_humidity,
                       temperature,
                       absolute_pressure,
                       average_wind_speed,
                       gust_wind_speed,
                       wind_direction,
                       station_id)
    VALUES(rec.download_timestamp,
           rec.time_stamp,
           rec.indoor_relative_humidity,
           rec.indoor_temperature,
           rec.relative_humidity,
           rec.temperature,
           rec.absolute_pressure,
           rec.average_wind_speed,
           rec.gust_wind_speed,
           rec.wind_direction,
           new_station_id
    ) RETURNING sample_id INTO temp_sample_id;

    INSERT INTO wh1080_sample(sample_id,
                              sample_interval,
                              record_number,
                              last_in_batch,
                              invalid_data,
                              total_rain,
                              rain_overflow)
    VALUES(temp_sample_id,
           rec.sample_interval,
           rec.record_number,
           rec.last_in_batch,
           rec.invalid_data,
           rec.total_rain,
           rec.rain_overflow);
  END LOOP;

END;
$$;

----------------------------------------------------------------------
-- VALIDATION --------------------------------------------------------
----------------------------------------------------------------------

do language plpgsql
$$
DECLARE
  old_sample_count integer;
  new_sample_count integer;
  wh_sample_count integer;
  bad_samples integer;
BEGIN

  RAISE NOTICE 'Performing validation pass...';

  select count(*) into old_sample_count from samples_v1;
  select count(*) into new_sample_count from sample;
  select count(*) into wh_sample_count from wh1080_sample;

  select count(*) into bad_samples
  from sample s
  inner join wh1080_sample ws on ws.sample_id = s.sample_id
  inner join samples_v1 old   on old.time_stamp = s.time_stamp
  where (
       old.sample_interval <> ws.sample_interval
    OR (old.sample_interval is null and ws.sample_interval is not null)
    OR (old.sample_interval is not null and ws.sample_interval is null)
    OR old.record_number <> ws.record_number -- not null
    OR old.download_timestamp <> s.download_timestamp
    OR (old.download_timestamp is null and s.download_timestamp is not null)
    OR (old.download_timestamp is not null and s.download_timestamp is null)
    OR old.last_in_batch <> ws.last_in_batch
    OR (old.last_in_batch is null and ws.last_in_batch is not null)
    OR (old.last_in_batch is not null and ws.last_in_batch is null)
    OR old.invalid_data <> ws.invalid_data -- not null
    OR old.indoor_relative_humidity <> s.indoor_relative_humidity
    OR (old.indoor_relative_humidity is null and s.indoor_relative_humidity is not null)
    OR (old.indoor_relative_humidity is not null and s.indoor_relative_humidity is null)
    OR round(old.indoor_temperature::numeric,2) <> round(s.indoor_temperature::numeric,2)
    OR (old.indoor_temperature is null and s.indoor_temperature is not null)
    OR (old.indoor_temperature is not null and s.indoor_temperature is null)
    OR old.relative_humidity <> s.relative_humidity
    OR (old.relative_humidity is null and s.relative_humidity is not null)
    OR (old.relative_humidity is not null and s.relative_humidity is null)
    OR round(old.temperature::numeric,2) <> round(s.temperature::numeric,2)
    OR (old.temperature is null and s.temperature is not null)
    OR (old.temperature is not null and s.temperature is null)
    OR round(old.dew_point::numeric,2) <> round(s.dew_point::numeric, 2)
    OR (old.dew_point is null and s.dew_point is not null)
    OR (old.dew_point is not null and s.dew_point is null)
    OR round(old.wind_chill::numeric, 2) <> round(s.wind_chill::numeric, 2)
    OR (old.wind_chill is null and s.wind_chill is not null)
    OR (old.wind_chill is not null and s.wind_chill is null)
    OR round(old.apparent_temperature::numeric, 2) <> round(s.apparent_temperature::numeric, 2)
    OR (old.apparent_temperature is null and s.apparent_temperature is not null)
    OR (old.apparent_temperature is not null and s.apparent_temperature is null)
    OR round(old.absolute_pressure::numeric, 2) <> round(s.absolute_pressure::numeric, 2)
    OR (old.absolute_pressure is null and s.absolute_pressure is not null)
    OR (old.absolute_pressure is not null and s.absolute_pressure is null)
    OR round(old.average_wind_speed::numeric, 2) <> round(s.average_wind_speed::numeric, 2)
    OR (old.average_wind_speed is null and s.average_wind_speed is not null)
    OR (old.average_wind_speed is not null and s.average_wind_speed is null)
    OR round(old.gust_wind_speed::numeric, 2) <> round(s.gust_wind_speed::numeric, 2)
    OR (old.gust_wind_speed is null and s.gust_wind_speed is not null)
    OR (old.gust_wind_speed is not null and s.gust_wind_speed is null)
    OR old.wind_direction <> s.wind_direction
    OR (old.wind_direction is null and s.wind_direction is not null)
    OR (old.wind_direction is not null and s.wind_direction is null)
    OR round(old.rainfall::numeric, 2) <> round(s.rainfall::numeric, 2)
    OR (old.rainfall is null and s.rainfall is not null)
    OR (old.rainfall is not null and s.rainfall is null)
    OR round(old.total_rain::numeric, 2) <> round(ws.total_rain::numeric, 2)
    OR (old.total_rain is null and ws.total_rain is not null)
    OR (old.total_rain is not null and ws.total_rain is null)
    OR old.rain_overflow <> ws.rain_overflow
    OR (old.rain_overflow is null and ws.rain_overflow is not null)
    OR (old.rain_overflow is not null and ws.rain_overflow is null)
  );

  -- Make sure *all* records were copied over
  if ((new_sample_count <> old_sample_count) or
      (wh_sample_count <> old_sample_count)) then
    RAISE EXCEPTION 'Validation failed: not all samples were migrated';
  end if;

  -- Make sure all records that were copied over were copied over exactly
  if (bad_samples > 0) then
    RAISE EXCEPTION 'Validation failed: samples were modified in migration';
  end if;
END;
$$;

----------------------------------------------------------------------
-- CLEAN UP ----------------------------------------------------------
----------------------------------------------------------------------

-- We've finished with this now. Get rid of it.
drop table samples_v1 cascade;

----------------------------------------------------------------------
-- END ---------------------------------------------------------------
----------------------------------------------------------------------

-- Uncomment this COMMIT statement when running the script manually.

--COMMIT;