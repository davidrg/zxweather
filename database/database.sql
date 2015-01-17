BEGIN;

----------------------------------------------------------------------
-- DOMAINS -----------------------------------------------------------
----------------------------------------------------------------------
-- A percentage.
CREATE DOMAIN rh_percentage
  AS integer;
COMMENT ON DOMAIN rh_percentage
  IS 'Relative Humidity percentage (0-100%)';

-- Valid wind directions
CREATE DOMAIN wind_direction
  AS character varying(3)
  NOT NULL
   CONSTRAINT wind_direction_check CHECK (((VALUE)::text = ANY ((ARRAY['N'::character varying, 'NNE'::character varying, 'NE'::character varying, 'ENE'::character varying, 'E'::character varying, 'ESE'::character varying, 'SE'::character varying, 'SSE'::character varying, 'S'::character varying, 'SSW'::character varying, 'SW'::character varying, 'WSW'::character varying, 'W'::character varying, 'WNW'::character varying, 'NW'::character varying, 'NNW'::character varying, 'INV'::character varying])::text[])));
COMMENT ON DOMAIN wind_direction
  IS 'Valid wind directions.';

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
INSERT INTO station_type(code, title) VALUES('DAVIS', 'Davis Vantage Pro2 or Vantage Vue');

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
  sort_order integer,
  message character varying,
  message_timestamp timestamptz,
  station_config character varying,
  CONSTRAINT pk_station PRIMARY KEY (station_id)
);

COMMENT ON TABLE station is 'Information about a weather station in this database';
COMMENT ON COLUMN station.code is 'A short code for the station. Used, for example, in the web interfaces URLs';
COMMENT ON COLUMN station.title is 'A user-readable title for the station';
COMMENT ON COLUMN station.description is 'A user-readable description for the station';
COMMENT ON COLUMN station.station_type_id is 'The type of hardware this station uses.';
COMMENT ON COLUMN station.sample_interval is 'How often (in seconds) new samples are logged.';
COMMENT ON COLUMN station.live_data_available is 'If live data is available from the live_data table for this station.';
comment on column station.sort_order is 'The order in which stations should be presented to the user';
comment on column station.message is 'A message which should be displayed where ever data for the station appears.';
comment on column station.message_timestamp is 'When the station message was last updated';
comment on column station.station_config is 'JSON document containing extra configuration data for the station. The structure of this document depends on the station type.';

-- Generic sample data. Anything that is specific to a particular station type
-- is in that station-specific table.
CREATE TABLE sample
(
  sample_id serial NOT NULL,
  download_timestamp timestamp with time zone, -- When this record was downloaded from the weather station
  time_stamp timestamp with time zone not null, -- The calculated date and time when this record was likely recorded by the weather station. How far out it could possibly be depends on the size of the sample interval, etc.
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
  wind_direction integer, -- Wind Direction in degrees
  rainfall real, -- Calculated rainfall. Calculation is based on total_rainfall and rain_overflow columns compared to the previous sample.
  station_id integer not null references station(station_id),
  CONSTRAINT pk_sample PRIMARY KEY (sample_id ),
  CONSTRAINT chk_wind_degrees CHECK (wind_direction >= 0 AND wind_direction < 360), -- Ensure wind direction is valid
  CONSTRAINT station_timestamp_unique UNIQUE(time_stamp, station_id) DEFERRABLE INITIALLY IMMEDIATE -- Prevent duplicate timestamps for a station
);
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
COMMENT ON COLUMN sample.wind_direction IS 'Prevailing wind direction in degrees.';
COMMENT ON COLUMN sample.rainfall IS 'Calculated rainfall in mm. Smallest recordable amount is 0.3mm. Calculation is based on total_rainfall and rain_overflow columns compared to the previous sample.';
COMMENT ON COLUMN sample.station_id IS 'The weather station this sample is for';

COMMENT ON CONSTRAINT station_timestamp_unique ON sample IS 'Each station can only have one sample for a given timestamp.';

ALTER TABLE sample
ADD CONSTRAINT chk_outdoor_relative_humidity
CHECK (relative_humidity > 0 and relative_humidity <= 100);
COMMENT ON CONSTRAINT chk_outdoor_relative_humidity ON sample
IS 'Ensure the outdoor relative humidity is in the range 0-100';

ALTER TABLE sample
ADD CONSTRAINT chk_indoor_relative_humidity
CHECK (indoor_relative_humidity > 0 and indoor_relative_humidity <= 100);
COMMENT ON CONSTRAINT chk_indoor_relative_humidity ON sample
IS 'Ensure the indoor relative humidity is in the range 0-100';

-- A table for data specific to WH1080-compatible hardware.
CREATE TABLE wh1080_sample
(
  sample_id integer not null primary key references sample(sample_id),
  sample_interval integer, -- Number of minutes since the previous sample
  record_number integer NOT NULL, -- The history slot in the weather stations circular buffer this record came from. Used for calculating the timestamp.
  last_in_batch boolean, -- If this was the last record in a batch of records downloaded from the weather station.
  invalid_data boolean NOT NULL, -- If the record from the weather stations "no sensor data received" flag is set.
  wind_direction wind_direction, -- Wind direction in text format
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
COMMENT ON COLUMN wh1080_sample.wind_direction IS 'Wind direction in text format';

-- A table for data specific to Davis-compatible hardware.
create table davis_sample (
  sample_id integer not null primary key references sample(sample_id),
  record_time integer not null,
  record_date integer not null,
  high_temperature float,
  low_temperature float,
  high_rain_rate float,
  solar_radiation float,
  wind_sample_count int,
  gust_wind_direction float,
  average_uv_index numeric(2,1),
  evapotranspiration float,
  high_solar_radiation float,
  high_uv_index numeric(2,1),
  forecast_rule_id int

  -- These columns are not currently stored as I've no way of testing them with
  -- my Vantage Vue and I doubt most people would have any of the extra sensors.
  -- I've left UV enabled as not recording that data at all is more likely to
  -- affect other potential users. Its still not displayed anywhere as I've
  -- no way of testing it.

  --leaf_temperature_A float,
  --leaf_temperature_B float,
  --leaf_wetness_A int,
  --leaf_wetness_B int,
  --soil_temperatures_A float,
  --soil_temperatures_B float,
  --soil_temperatures_C float,
  --soil_temperatures_D float,
  --extra_humidity_A rh_percentage,
  --extra_humidity_B rh_percentage,
  --extra_temperature_A float,
  --extra_temperature_B float,
  --extra_temperature_C float,
  --soil_moisture_A int,
  --soil_moisture_B int,
  --soil_moisture_C int,
  --soil_moisture_D int
);
comment on table davis_sample is 'Data for samples taken from Davis-compatbile hardware';
comment on column davis_sample.record_time is 'The time value from the record';
comment on column davis_sample.record_date is 'The date value from the record';
comment on column davis_sample.high_temperature is 'The highest outside temperature over the archive period.';
comment on column davis_sample.low_temperature is 'The lowest outside temperature over the archive period.';
comment on column davis_sample.high_rain_rate is 'The highest rain rate over the archive period or the rain rate shown on the console at the end of the period if there was no rain. Unit is in mm/h';
comment on column davis_sample.solar_radiation is 'The average solar radiation over the archive period in watts per square meter';
comment on column davis_sample.wind_sample_count is 'The number of wind sample counts over the archive period. This can be used for calculating reception quality as wind samples are present in most packets received';
comment on column davis_sample.gust_wind_direction is 'Direction of the gust wind speed';
comment on column davis_sample.average_uv_index is 'Average UV Index';
comment on column davis_sample.evapotranspiration is 'Evapotranspiration accumulated over the last hour. Only records on the hour will have a non-zero value.';
comment on column davis_sample.high_solar_radiation is 'Highest solar radiation value over the archive period in watts per square meter';
comment on column davis_sample.high_uv_index is 'Highest UV index observed over the archive period in watts per square meter';
comment on column davis_sample.forecast_rule_id is 'ID for the forecast rule at the end of the archive period.';

create table davis_forecast_rule(
  rule_id integer not null primary key,
  description character varying
);

comment on table davis_forecast_rule is 'Forecast rule descriptions for Davis-compatible hardware';
comment on column davis_forecast_rule.rule_id is 'The Rule Id used in archive samples and live data';
comment on column davis_forecast_rule.description is 'A text description of the forecast';

insert into davis_forecast_rule values(0, 'Mostly clear and cooler.');
insert into davis_forecast_rule values(1, 'Mostly clear with little temperature change.');
insert into davis_forecast_rule values(2, 'Mostly clear for 12 hours with little temperature change.');
insert into davis_forecast_rule values(3, 'Mostly clear for 12 to 24 hours and cooler.');
insert into davis_forecast_rule values(4, 'Mostly clear with little temperature change.');
insert into davis_forecast_rule values(5, 'Partly cloudy and cooler.');
insert into davis_forecast_rule values(6, 'Partly cloudy with little temperature change.');
insert into davis_forecast_rule values(7, 'Partly cloudy with little temperature change.');
insert into davis_forecast_rule values(8, 'Mostly clear and warmer.');
insert into davis_forecast_rule values(9, 'Partly cloudy with little temperature change.');
insert into davis_forecast_rule values(10, 'Partly cloudy with little temperature change.');
insert into davis_forecast_rule values(11, 'Mostly clear with little temperature change.');
insert into davis_forecast_rule values(12, 'Increasing clouds and warmer. Precipitation possible within 24 to 48 hours.');
insert into davis_forecast_rule values(13, 'Partly cloudy with little temperature change.');
insert into davis_forecast_rule values(14, 'Mostly clear with little temperature change.');
insert into davis_forecast_rule values(15, 'Increasing clouds with little temperature change. Precipitation possible within 24 hours.');
insert into davis_forecast_rule values(16, 'Mostly clear with little temperature change.');
insert into davis_forecast_rule values(17, 'Partly cloudy with little temperature change.');
insert into davis_forecast_rule values(18, 'Mostly clear with little temperature change.');
insert into davis_forecast_rule values(19, 'Increasing clouds with little temperature change. Precipitation possible within 12 hours.');
insert into davis_forecast_rule values(20, 'Mostly clear with little temperature change.');
insert into davis_forecast_rule values(21, 'Partly cloudy with little temperature change.');
insert into davis_forecast_rule values(22, 'Mostly clear with little temperature change.');
insert into davis_forecast_rule values(23, 'Increasing clouds and warmer. Precipitation possible within 24 hours.');
insert into davis_forecast_rule values(24, 'Mostly clear and warmer. Increasing winds.');
insert into davis_forecast_rule values(25, 'Partly cloudy with little temperature change.');
insert into davis_forecast_rule values(26, 'Mostly clear with little temperature change.');
insert into davis_forecast_rule values(27, 'Increasing clouds and warmer. Precipitation possible within 12 hours. Increasing winds.');
insert into davis_forecast_rule values(28, 'Mostly clear and warmer. Increasing winds.');
insert into davis_forecast_rule values(29, 'Increasing clouds and warmer.');
insert into davis_forecast_rule values(30, 'Partly cloudy with little temperature change.');
insert into davis_forecast_rule values(31, 'Mostly clear with little temperature change.');
insert into davis_forecast_rule values(32, 'Increasing clouds and warmer. Precipitation possible within 12 hours. Increasing winds.');
insert into davis_forecast_rule values(33, 'Mostly clear and warmer. Increasing winds.');
insert into davis_forecast_rule values(34, 'Increasing clouds and warmer.');
insert into davis_forecast_rule values(35, 'Partly cloudy with little temperature change.');
insert into davis_forecast_rule values(36, 'Mostly clear with little temperature change.');
insert into davis_forecast_rule values(37, 'Increasing clouds and warmer. Precipitation possible within 12 hours. Increasing winds.');
insert into davis_forecast_rule values(38, 'Partly cloudy with little temperature change.');
insert into davis_forecast_rule values(39, 'Mostly clear with little temperature change.');
insert into davis_forecast_rule values(40, 'Mostly clear and warmer. Precipitation possible within 48 hours.');
insert into davis_forecast_rule values(41, 'Mostly clear and warmer.');
insert into davis_forecast_rule values(42, 'Partly cloudy with little temperature change.');
insert into davis_forecast_rule values(43, 'Mostly clear with little temperature change.');
insert into davis_forecast_rule values(44, 'Increasing clouds with little temperature change. Precipitation possible within 24 to 48 hours.');
insert into davis_forecast_rule values(45, 'Increasing clouds with little temperature change.');
insert into davis_forecast_rule values(46, 'Partly cloudy with little temperature change.');
insert into davis_forecast_rule values(47, 'Mostly clear with little temperature change.');
insert into davis_forecast_rule values(48, 'Increasing clouds and warmer. Precipitation possible within 12 to 24 hours.');
insert into davis_forecast_rule values(49, 'Partly cloudy with little temperature change.');
insert into davis_forecast_rule values(50, 'Mostly clear with little temperature change.');
insert into davis_forecast_rule values(51, 'Increasing clouds and warmer. Precipitation possible within 12 to 24 hours. Windy.');
insert into davis_forecast_rule values(52, 'Partly cloudy with little temperature change.');
insert into davis_forecast_rule values(53, 'Mostly clear with little temperature change.');
insert into davis_forecast_rule values(54, 'Increasing clouds and warmer. Precipitation possible within 12 to 24 hours. Windy.');
insert into davis_forecast_rule values(55, 'Partly cloudy with little temperature change.');
insert into davis_forecast_rule values(56, 'Mostly clear with little temperature change.');
insert into davis_forecast_rule values(57, 'Increasing clouds and warmer. Precipitation possible within 6 to 12 hours.');
insert into davis_forecast_rule values(58, 'Partly cloudy with little temperature change.');
insert into davis_forecast_rule values(59, 'Mostly clear with little temperature change.');
insert into davis_forecast_rule values(60, 'Increasing clouds and warmer. Precipitation possible within 6 to 12 hours. Windy.');
insert into davis_forecast_rule values(61, 'Partly cloudy with little temperature change.');
insert into davis_forecast_rule values(62, 'Mostly clear with little temperature change.');
insert into davis_forecast_rule values(63, 'Increasing clouds and warmer. Precipitation possible within 12 to 24 hours. Windy.');
insert into davis_forecast_rule values(64, 'Partly cloudy with little temperature change.');
insert into davis_forecast_rule values(65, 'Mostly clear with little temperature change.');
insert into davis_forecast_rule values(66, 'Increasing clouds and warmer. Precipitation possible within 12 hours.');
insert into davis_forecast_rule values(67, 'Partly cloudy with little temperature change.');
insert into davis_forecast_rule values(68, 'Mostly clear with little temperature change.');
insert into davis_forecast_rule values(69, 'Increasing clouds and warmer. Precipitation likley.');
insert into davis_forecast_rule values(70, 'Clearing and cooler. Precipitation ending within 6 hours.');
insert into davis_forecast_rule values(71, 'Partly cloudy with little temperature change.');
insert into davis_forecast_rule values(72, 'Clearing and cooler. Precipitation ending within 6 hours.');
insert into davis_forecast_rule values(73, 'Mostly clear with little temperature change.');
insert into davis_forecast_rule values(74, 'Clearing and cooler. Precipitation ending within 6 hours.');
insert into davis_forecast_rule values(75, 'Partly cloudy and cooler.');
insert into davis_forecast_rule values(76, 'Partly cloudy with little temperature change.');
insert into davis_forecast_rule values(77, 'Mostly clear and cooler.');
insert into davis_forecast_rule values(78, 'Clearing and cooler. Precipitation ending within 6 hours.');
insert into davis_forecast_rule values(79, 'Mostly clear with little temperature change.');
insert into davis_forecast_rule values(80, 'Clearing and cooler. Precipitation ending within 6 hours.');
insert into davis_forecast_rule values(81, 'Mostly clear and cooler.');
insert into davis_forecast_rule values(82, 'Partly cloudy with little temperature change.');
insert into davis_forecast_rule values(83, 'Mostly clear with little temperature change.');
insert into davis_forecast_rule values(84, 'Increasing clouds with little temperature change. Precipitation possible within 24 hours.');
insert into davis_forecast_rule values(85, 'Mostly cloudy and cooler. Precipitation continuing.');
insert into davis_forecast_rule values(86, 'Partly cloudy with little temperature change.');
insert into davis_forecast_rule values(87, 'Mostly clear with little temperature change.');
insert into davis_forecast_rule values(88, 'Mostly cloudy and cooler. Precipitation likely.');
insert into davis_forecast_rule values(89, 'Mostly cloudy with little temperature change. Precipitation continuing.');
insert into davis_forecast_rule values(90, 'Mostly cloudy with little temperature change. Precipitation likely.');
insert into davis_forecast_rule values(91, 'Partly cloudy with little temperature change.');
insert into davis_forecast_rule values(92, 'Mostly clear with little temperature change.');
insert into davis_forecast_rule values(93, 'Increasing clouds and cooler. Precipitation possible and windy within 6 hours.');
insert into davis_forecast_rule values(94, 'Increasing clouds with little temperature change. Precipitation possible and windy within 6 hours.');
insert into davis_forecast_rule values(95, 'Mostly cloudy and cooler. Precipitation continuing. Increasing winds.');
insert into davis_forecast_rule values(96, 'Partly cloudy with little temperature change.');
insert into davis_forecast_rule values(97, 'Mostly clear with little temperature change.');
insert into davis_forecast_rule values(98, 'Mostly cloudy and cooler. Precipitation likely. Increasing winds.');
insert into davis_forecast_rule values(99, 'Mostly cloudy with little temperature change. Precipitation continuing. Increasing winds.');
insert into davis_forecast_rule values(100, 'Mostly cloudy with little temperature change. Precipitation likely. Increasing winds.');
insert into davis_forecast_rule values(101, 'Partly cloudy with little temperature change.');
insert into davis_forecast_rule values(102, 'Mostly clear with little temperature change.');
insert into davis_forecast_rule values(103, 'Increasing clouds and cooler. Precipitation possible within 12 to 24 hours possible wind shift to the W, NW, or N.');
insert into davis_forecast_rule values(104, 'Increasing clouds with little temperature change. Precipitation possible within 12 to 24 hours possible wind shift to the W, NW, or N.');
insert into davis_forecast_rule values(105, 'Partly cloudy with little temperature change.');
insert into davis_forecast_rule values(106, 'Mostly clear with little temperature change.');
insert into davis_forecast_rule values(107, 'Increasing clouds and cooler. Precipitation possible within 6 hours possible wind shift to the W, NW, or N.');
insert into davis_forecast_rule values(108, 'Increasing clouds with little temperature change. Precipitation possible within 6 hours possible wind shift to the W, NW, or N.');
insert into davis_forecast_rule values(109, 'Mostly cloudy and cooler. Precipitation ending within 12 hours possible wind shift to the W, NW, or N.');
insert into davis_forecast_rule values(110, 'Mostly cloudy and cooler. Possible wind shift to the W, NW, or N.');
insert into davis_forecast_rule values(111, 'Mostly cloudy with little temperature change. Precipitation ending within 12 hours possible wind shift to the W, NW, or N.');
insert into davis_forecast_rule values(112, 'Mostly cloudy with little temperature change. Possible wind shift to the W, NW, or N.');
insert into davis_forecast_rule values(113, 'Mostly cloudy and cooler. Precipitation ending within 12 hours possible wind shift to the W, NW, or N.');
insert into davis_forecast_rule values(114, 'Partly cloudy with little temperature change.');
insert into davis_forecast_rule values(115, 'Mostly clear with little temperature change.');
insert into davis_forecast_rule values(116, 'Mostly cloudy and cooler. Precipitation possible within 24 hours possible wind shift to the W, NW, or N.');
insert into davis_forecast_rule values(117, 'Mostly cloudy with little temperature change. Precipitation ending within 12 hours possible wind shift to the W, NW, or N.');
insert into davis_forecast_rule values(118, 'Mostly cloudy with little temperature change. Precipitation possible within 24 hours possible wind shift to the W, NW, or N.');
insert into davis_forecast_rule values(119, 'Clearing, cooler and windy. Precipitation ending within 6 hours.');
insert into davis_forecast_rule values(120, 'Clearing, cooler and windy.');
insert into davis_forecast_rule values(121, 'Mostly cloudy and cooler. Precipitation ending within 6 hours. Windy with possible wind shift to the W, NW, or N.');
insert into davis_forecast_rule values(122, 'Mostly cloudy and cooler. Windy with possible wind shift o the W, NW, or N.');
insert into davis_forecast_rule values(123, 'Clearing, cooler and windy.');
insert into davis_forecast_rule values(124, 'Partly cloudy with little temperature change.');
insert into davis_forecast_rule values(125, 'Mostly clear with little temperature change.');
insert into davis_forecast_rule values(126, 'Mostly cloudy with little temperature change. Precipitation possible within 12 hours. Windy.');
insert into davis_forecast_rule values(127, 'Partly cloudy with little temperature change.');
insert into davis_forecast_rule values(128, 'Mostly clear with little temperature change.');
insert into davis_forecast_rule values(129, 'Increasing clouds and cooler. Precipitation possible within 12 hours, possibly heavy at times. Windy.');
insert into davis_forecast_rule values(130, 'Mostly cloudy and cooler. Precipitation ending within 6 hours. Windy.');
insert into davis_forecast_rule values(131, 'Partly cloudy with little temperature change.');
insert into davis_forecast_rule values(132, 'Mostly clear with little temperature change.');
insert into davis_forecast_rule values(133, 'Mostly cloudy and cooler. Precipitation possible within 12 hours. Windy.');
insert into davis_forecast_rule values(134, 'Mostly cloudy and cooler. Precipitation ending in 12 to 24 hours.');
insert into davis_forecast_rule values(135, 'Mostly cloudy and cooler.');
insert into davis_forecast_rule values(136, 'Mostly cloudy and cooler. Precipitation continuing, possible heavy at times. Windy.');
insert into davis_forecast_rule values(137, 'Partly cloudy with little temperature change.');
insert into davis_forecast_rule values(138, 'Mostly clear with little temperature change.');
insert into davis_forecast_rule values(139, 'Mostly cloudy and cooler. Precipitation possible within 6 to 12 hours. Windy.');
insert into davis_forecast_rule values(140, 'Mostly cloudy with little temperature change. Precipitation continuing, possibly heavy at times. Windy.');
insert into davis_forecast_rule values(141, 'Partly cloudy with little temperature change.');
insert into davis_forecast_rule values(142, 'Mostly clear with little temperature change.');
insert into davis_forecast_rule values(143, 'Mostly cloudy with little temperature change. Precipitation possible within 6 to 12 hours. Windy.');
insert into davis_forecast_rule values(144, 'Partly cloudy with little temperature change.');
insert into davis_forecast_rule values(145, 'Mostly clear with little temperature change.');
insert into davis_forecast_rule values(146, 'Increasing clouds with little temperature change. Precipitation possible within 12 hours, possibly heavy at times. Windy.');
insert into davis_forecast_rule values(147, 'Mostly cloudy and cooler. Windy.');
insert into davis_forecast_rule values(148, 'Mostly cloudy and cooler. Precipitation continuing, possibly heavy at times. Windy.');
insert into davis_forecast_rule values(149, 'Partly cloudy with little temperature change.');
insert into davis_forecast_rule values(150, 'Mostly clear with little temperature change.');
insert into davis_forecast_rule values(151, 'Mostly cloudy and cooler. Precipitation likely, possibly heavy at times. Windy.');
insert into davis_forecast_rule values(152, 'Mostly cloudy with little temperature change. Precipitation continuing, possibly heavy at times. Windy.');
insert into davis_forecast_rule values(153, 'Mostly cloudy with little temperature change. Precipitation likely, possibly heavy at times. Windy.');
insert into davis_forecast_rule values(154, 'Partly cloudy with little temperature change.');
insert into davis_forecast_rule values(155, 'Mostly clear with little temperature change.');
insert into davis_forecast_rule values(156, 'Increasing clouds and cooler. Precipitation possible within 6 hours. Windy.');
insert into davis_forecast_rule values(157, 'Increasing clouds with little temperature change. Precipitation possible within 6 hours. Windy');
insert into davis_forecast_rule values(158, 'Increasing clouds and cooler. Precipitation continuing. Windy with possible wind shift to the W, NW, or N.');
insert into davis_forecast_rule values(159, 'Partly cloudy with little temperature change.');
insert into davis_forecast_rule values(160, 'Mostly clear with little temperature change.');
insert into davis_forecast_rule values(161, 'Mostly cloudy and cooler. Precipitation likely. Windy with possible wind shift to the W, NW, or N.');
insert into davis_forecast_rule values(162, 'Mostly cloudy with little temperature change. Precipitation continuing. Windy with possible wind shift to the W, NW, or N.');
insert into davis_forecast_rule values(163, 'Mostly cloudy with little temperature change. Precipitation likely. Windy with possible wind shift to the W, NW, or N.');
insert into davis_forecast_rule values(164, 'Increasing clouds and cooler. Precipitation possible within 6 hours. Windy with possible wind shift to the W, NW, or N.');
insert into davis_forecast_rule values(165, 'Partly cloudy with little temperature change.');
insert into davis_forecast_rule values(166, 'Mostly clear with little temperature change.');
insert into davis_forecast_rule values(167, 'Increasing clouds and cooler. Precipitation possible within 6 hours possible wind shift to the W, NW, or N.');
insert into davis_forecast_rule values(168, 'Increasing clouds with little temperature change. Precipitation possible within 6 hours. Windy with possible wind shift to the W, NW, or N.');
insert into davis_forecast_rule values(169, 'Increasing clouds with little temperature change. Precipitation possible within 6 hours possible wind shift to the W, NW, or N.');
insert into davis_forecast_rule values(170, 'Partly cloudy with little temperature change.');
insert into davis_forecast_rule values(171, 'Mostly clear with little temperature change.');
insert into davis_forecast_rule values(172, 'Increasing clouds and cooler. Precipitation possible within 6 hours. Windy with possible wind shift to the W, NW, or N.');
insert into davis_forecast_rule values(173, 'Increasing clouds with little temperature change. Precipitation possible within 6 hours. Windy with possible wind shift to the W, NW, or N.');
insert into davis_forecast_rule values(174, 'Partly cloudy with little temperature change.');
insert into davis_forecast_rule values(175, 'Mostly clear with little temperature change.');
insert into davis_forecast_rule values(176, 'Increasing clouds and cooler. Precipitation possible within 12 to 24 hours. Windy with possible wind shift to the W, NW, or N.');
insert into davis_forecast_rule values(177, 'Increasing clouds with little temperature change. Precipitation possible within 12 to 24 hours. Windy with possible wind shift to the W, NW, or N.');
insert into davis_forecast_rule values(178, 'Mostly cloudy and cooler. Precipitation possibly heavy at times and ending within 12 hours. Windy with possible wind shift to the W, NW, or N.');
insert into davis_forecast_rule values(179, 'Partly cloudy with little temperature change.');
insert into davis_forecast_rule values(180, 'Mostly clear with little temperature change.');
insert into davis_forecast_rule values(181, 'Mostly cloudy and cooler. Precipitation possible within 6 to 12 hours, possibly heavy at times. Windy with possible wind shift to the W, NW, or N.');
insert into davis_forecast_rule values(182, 'Mostly cloudy with little temperature change. Precipitation ending within 12 hours. Windy with possible wind shift to the W, NW, or N.');
insert into davis_forecast_rule values(183, 'Mostly cloudy with little temperature change. Precipitation possible within 6 to 12 hours, possibly heavy at times. Windy with possible wind shift to the W, NW, or N.');
insert into davis_forecast_rule values(184, 'Mostly cloudy and cooler. Precipitation continuing.');
insert into davis_forecast_rule values(185, 'Partly cloudy with little temperature change.');
insert into davis_forecast_rule values(186, 'Mostly clear with little temperature change.');
insert into davis_forecast_rule values(187, 'Mostly cloudy and cooler. Precipitation likely. Windy with possible wind shift to the W, NW, or N.');
insert into davis_forecast_rule values(188, 'Mostly cloudy with little temperature change. Precipitation continuing.');
insert into davis_forecast_rule values(189, 'Mostly cloudy with little temperature change. Precipitation likely.');
insert into davis_forecast_rule values(190, 'Partly cloudy with little temperature change.');
insert into davis_forecast_rule values(191, 'Mostly clear with little temperature change.');
insert into davis_forecast_rule values(192, 'Mostly cloudy and cooler. Precipitation possible within 12 hours, possibly heavy at times. Windy.');
insert into davis_forecast_rule values(193, 'FORECAST REQUIRES 3 HOURS OF RECENT DATA');
insert into davis_forecast_rule values(194, 'Mostly clear and cooler.');
insert into davis_forecast_rule values(195, 'Mostly clear and cooler.');
insert into davis_forecast_rule values(196, 'Mostly clear and cooler.');

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
  wind_direction integer -- Wind Direction.
);
COMMENT ON TABLE live_data
  IS 'Live data from the weather stations. Contains only a single record.';
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

-- Additional live data available from Davis-compatible hardware.
create table davis_live_data (
  station_id integer not null primary key references station(station_id),
  bar_trend integer,
  rain_rate float,
  storm_rain float,
  current_storm_start_date date,
  transmitter_battery int,
  console_battery_voltage float,
  forecast_icon int,
  forecast_rule_id int,
  uv_index numeric(2,1),
  solar_radiation int
);
comment on table davis_live_data is 'Additional live data available from Davis-compatible hardware';
comment on column davis_live_data.bar_trend is 'Barometer trend. -60 is falling rapidly, -20 is falling slowly, 0 is steady, 20 is rising slowly, 60 is rising rapidly.';
comment on column davis_live_data.rain_rate is 'Rain rate in mm/hr';
comment on column davis_live_data.storm_rain is 'Storm rain in mm';
comment on column davis_live_data.current_storm_start_date is 'Start date of current storm.';
comment on column davis_live_data.transmitter_battery is 'Transmitter battery status';
comment on column davis_live_data.console_battery_voltage is 'Console battery voltage';
comment on column davis_live_data.forecast_icon is 'Forecast icon';
comment on column davis_live_data.forecast_rule_id is 'Current forecast rule. See davis_forecast_rule table for values';
comment on column davis_live_data.uv_index is 'Latest UV index reading';
comment on column davis_live_data.solar_radiation is 'Latest solar radiation reading in watt/meter squared';

-- A table to store some basic information about the database (such as schema
-- version).
CREATE TABLE db_info
(
  k varchar primary key not null,
  v character varying
);

COMMENT ON TABLE db_info IS 'Basic database info (key/value data)';
COMMENT ON COLUMN db_info.k is 'Data key';
COMMENT ON COLUMN db_info.v is 'Data value';

-- This is schema revision 3 (adds Davis hardware support).
insert into db_info(k,v) VALUES('DB_VERSION','3');

-- And it its not compatible with anything older than zxweather 0.2.0
insert into db_info(k,v) VALUES('MIN_VER_MAJ', '0');
insert into db_info(k,v) VALUES('MIN_VER_MIN', '2');
insert into db_info(k,v) VALUES('MIN_VER_REV', '0');

-- Versions of the admin tool before v1.0.0 don't know how to initially populate
-- the davis_live_data table when creating a new DAVIS-type station. This will
-- prevent older versions of the admin tool from being used with the database.
insert into db_info(k,v) values('ADMIN_TOOL_MIN_VER_MAJ','1');
insert into db_info(k,v) values('ADMIN_TOOL_MIN_VER_MIN','0');
insert into db_info(k,v) values('ADMIN_TOOL_MIN_VER_REV','0');

----------------------------------------------------------------------
-- INDICIES ----------------------------------------------------------
----------------------------------------------------------------------

-- Cuts recalculating rainfall for 3000 records from 4200ms to 110ms.
CREATE INDEX idx_time_stamp
   ON sample (time_stamp ASC NULLS LAST);
COMMENT ON INDEX idx_time_stamp
  IS 'To make operations such as recalculating rainfall a little quicker.';

-- Don't allow duplicate timestamps within a station.
CREATE UNIQUE INDEX idx_station_timestamp_unique
ON sample
USING btree
(time_stamp, station_id);
COMMENT ON INDEX idx_station_timestamp_unique
IS 'Each station can only have one sample for a given timestamp.';

-- For the latest_record_number view.
--CREATE INDEX idx_latest_record
--   ON sample (time_stamp DESC NULLS FIRST, record_number DESC NULLS FIRST);
--COMMENT ON INDEX idx_latest_record
--  IS 'Index for the latest_record_number view';

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

CREATE OR REPLACE VIEW daily_indoor_records AS
 SELECT data.date_stamp,
        data.station_id,
        data.min_indoor_temperature,
        max(data.min_indoor_temperature_ts) AS min_indoor_temperature_ts,
        data.max_indoor_temperature,
        max(data.max_indoor_temperature_ts) AS max_indoor_temperature_ts,
        data.min_indoor_humidity,
        max(data.min_indoor_humidity_ts) AS min_indoor_humidity_ts,
        data.max_indoor_humidity,
        max(data.max_indoor_humidity_ts) AS max_indoor_humidity_ts
   FROM (SELECT dr.date_stamp,
                dr.station_id,
                dr.max_indoor_temperature,
                CASE
                    WHEN s.indoor_temperature = dr.max_indoor_temperature THEN s.time_stamp
                    ELSE NULL::timestamp with time zone
                END AS max_indoor_temperature_ts,
                dr.min_indoor_temperature,
                CASE
                    WHEN s.indoor_temperature = dr.min_indoor_temperature THEN s.time_stamp
                    ELSE NULL::timestamp with time zone
                END AS min_indoor_temperature_ts,
                dr.max_indoor_humidity,
                CASE
                    WHEN s.indoor_relative_humidity = dr.max_indoor_humidity THEN s.time_stamp
                    ELSE NULL::timestamp with time zone
                END AS max_indoor_humidity_ts,
                dr.min_indoor_humidity,
                CASE
                    WHEN s.indoor_relative_humidity = dr.min_indoor_humidity THEN s.time_stamp
                    ELSE NULL::timestamp with time zone
                END AS min_indoor_humidity_ts
           FROM sample s
      JOIN ( SELECT date(s.time_stamp) AS date_stamp, s.station_id,
                    min(s.indoor_temperature) AS min_indoor_temperature,
                    max(s.indoor_temperature) AS max_indoor_temperature,
                    min(s.indoor_relative_humidity) AS min_indoor_humidity,
                    max(s.indoor_relative_humidity) AS max_indoor_humidity
               FROM sample s
             GROUP BY date(s.time_stamp),
                      s.station_id
           ) dr ON date(s.time_stamp) = dr.date_stamp
     WHERE s.indoor_temperature = dr.max_indoor_temperature
        OR s.indoor_temperature = dr.min_indoor_temperature
        OR s.indoor_relative_humidity = dr.max_indoor_humidity
        OR s.indoor_relative_humidity = dr.min_indoor_humidity ) data
  GROUP BY data.date_stamp,
           data.station_id,
           data.max_indoor_temperature,
           data.min_indoor_temperature,
           data.max_indoor_humidity,
           data.min_indoor_humidity
  ORDER BY data.date_stamp DESC;

COMMENT ON VIEW daily_indoor_records
IS 'Indoor minimum and maximum temperature and humidity records for each day.';


CREATE OR REPLACE VIEW monthly_indoor_records AS
 SELECT data.date_stamp,
        data.station_id,
        data.min_indoor_temperature,
        max(data.min_indoor_temperature_ts) AS min_indoor_temperature_ts,
        data.max_indoor_temperature,
        max(data.max_indoor_temperature_ts) AS max_indoor_temperature_ts,
        data.min_indoor_humidity,
        max(data.min_indoor_humidity_ts) AS min_indoor_humidity_ts,
        data.max_indoor_humidity,
        max(data.max_indoor_humidity_ts) AS max_indoor_humidity_ts
   FROM (SELECT dr.date_stamp,
                dr.station_id,
                dr.max_indoor_temperature,
                CASE
                    WHEN s.indoor_temperature = dr.max_indoor_temperature THEN s.time_stamp
                    ELSE NULL::timestamp with time zone
                END AS max_indoor_temperature_ts,
                dr.min_indoor_temperature,
                CASE
                    WHEN s.indoor_temperature = dr.min_indoor_temperature THEN s.time_stamp
                    ELSE NULL::timestamp with time zone
                END AS min_indoor_temperature_ts,
                dr.max_indoor_humidity,
                CASE
                    WHEN s.indoor_relative_humidity = dr.max_indoor_humidity THEN s.time_stamp
                    ELSE NULL::timestamp with time zone
                END AS max_indoor_humidity_ts,
                dr.min_indoor_humidity,
                CASE
                    WHEN s.indoor_relative_humidity = dr.min_indoor_humidity THEN s.time_stamp
                    ELSE NULL::timestamp with time zone
                END AS min_indoor_humidity_ts
           FROM sample s
      JOIN ( SELECT date(date_trunc('month', s.time_stamp)) AS date_stamp, s.station_id,
                    min(s.indoor_temperature) AS min_indoor_temperature,
                    max(s.indoor_temperature) AS max_indoor_temperature,
                    min(s.indoor_relative_humidity) AS min_indoor_humidity,
                    max(s.indoor_relative_humidity) AS max_indoor_humidity
               FROM sample s
             GROUP BY date_stamp, s.station_id) dr
        ON date(date_trunc('month', s.time_stamp)) = dr.date_stamp
     WHERE s.indoor_temperature = dr.max_indoor_temperature
        OR s.indoor_temperature = dr.min_indoor_temperature
        OR s.indoor_relative_humidity = dr.max_indoor_humidity
        OR s.indoor_relative_humidity = dr.min_indoor_humidity ) data
  GROUP BY data.date_stamp,
           data.station_id,
           data.max_indoor_temperature,
           data.min_indoor_temperature,
           data.max_indoor_humidity,
           data.min_indoor_humidity
  ORDER BY data.date_stamp DESC;

COMMENT ON VIEW monthly_indoor_records
IS 'Indoor minimum and maximum temperature and humidity records for each month.';


CREATE OR REPLACE VIEW yearly_indoor_records AS
 SELECT data.year_stamp,
        data.station_id,
        data.min_indoor_temperature,
        max(data.min_indoor_temperature_ts) AS min_indoor_temperature_ts,
        data.max_indoor_temperature,
        max(data.max_indoor_temperature_ts) AS max_indoor_temperature_ts,
        data.min_indoor_humidity,
        max(data.min_indoor_humidity_ts) AS min_indoor_humidity_ts,
        data.max_indoor_humidity,
        max(data.max_indoor_humidity_ts) AS max_indoor_humidity_ts
   FROM (SELECT dr.year_stamp,
                dr.station_id,
                dr.max_indoor_temperature,
                CASE
                    WHEN s.indoor_temperature = dr.max_indoor_temperature THEN s.time_stamp
                    ELSE NULL::timestamp with time zone
                END AS max_indoor_temperature_ts,
                dr.min_indoor_temperature,
                CASE
                    WHEN s.indoor_temperature = dr.min_indoor_temperature THEN s.time_stamp
                    ELSE NULL::timestamp with time zone
                END AS min_indoor_temperature_ts,
                dr.max_indoor_humidity,
                CASE
                    WHEN s.indoor_relative_humidity = dr.max_indoor_humidity THEN s.time_stamp
                    ELSE NULL::timestamp with time zone
                END AS max_indoor_humidity_ts,
                dr.min_indoor_humidity,
                CASE
                    WHEN s.indoor_relative_humidity = dr.min_indoor_humidity THEN s.time_stamp
                    ELSE NULL::timestamp with time zone
                END AS min_indoor_humidity_ts
           FROM sample s
      JOIN ( SELECT extract(year from s.time_stamp) as year_stamp, s.station_id,
                    min(s.indoor_temperature) AS min_indoor_temperature,
                    max(s.indoor_temperature) AS max_indoor_temperature,
                    min(s.indoor_relative_humidity) AS min_indoor_humidity,
                    max(s.indoor_relative_humidity) AS max_indoor_humidity
               FROM sample s
             GROUP BY year_stamp, s.station_id) dr
        ON extract(year from s.time_stamp) = dr.year_stamp
     WHERE s.indoor_temperature = dr.max_indoor_temperature
        OR s.indoor_temperature = dr.min_indoor_temperature
        OR s.indoor_relative_humidity = dr.max_indoor_humidity
        OR s.indoor_relative_humidity = dr.min_indoor_humidity ) data
  GROUP BY data.year_stamp,
           data.station_id,
           data.max_indoor_temperature,
           data.min_indoor_temperature,
           data.max_indoor_humidity,
           data.min_indoor_humidity
  ORDER BY data.year_stamp DESC;

COMMENT ON VIEW yearly_indoor_records
IS 'Indoor minimum and maximum temperature and humidity records for each year.';

----------------------------------------------------------------------
-- FUNCTIONS ---------------------------------------------------------
----------------------------------------------------------------------

-- Function to calculate dew point
CREATE OR REPLACE FUNCTION dew_point(temperature real, relative_humidity integer)
RETURNS real AS
$BODY$
DECLARE
  a real := 17.271;
  b real := 237.7; -- in degrees C
  gamma real;
  result real;
BEGIN
  gamma = ((a * temperature) / (b + temperature)) + ln(relative_humidity::real / 100.0);
  result = (b * gamma) / (a - gamma);
  return result;
END; $BODY$
LANGUAGE plpgsql IMMUTABLE;
COMMENT ON FUNCTION dew_point(real, integer) IS 'Calculates the approximate dew point given temperature relative humidity. The calculation is based on the August-Roche-Magnus approximation for the saturation vapor pressure of water in air as a function of temperature. It is valid for input temperatures 0 to 60 degC and dew points 0 to 50 deg C.';

-- Function to calculate wind chill
CREATE OR REPLACE FUNCTION wind_chill(temperature real, wind_speed real)
RETURNS real AS
$BODY$
DECLARE
  wind_kph real;
  result real;
BEGIN
  -- wind_speed is in m/s. Multiply by 3.6 to get km/h
  wind_kph := wind_speed * 3.6;

  -- This calculation is undefined for temperatures above 10degC or wind speeds below 4.8km/h
  if wind_kph <= 4.8 or temperature > 10.0 then
        return temperature;
  end if;

  result := 13.12 + (temperature * 0.6215) + (((0.3965 * temperature) - 11.37) * pow(wind_kph, 0.16));

  -- Wind chill is always lower than the temperature.
  if result > temperature then
        return temperature;
  end if;

  return result;
END; $BODY$
LANGUAGE plpgsql IMMUTABLE;
COMMENT ON FUNCTION wind_chill(real, real) IS 'Calculates the north american wind chill index. Formula from http://en.wikipedia.org/wiki/Wind_chill.';

-- Calculates the apparent temperature
CREATE OR REPLACE FUNCTION apparent_temperature(temperature real, wind_speed real, relative_humidity real)
RETURNS real AS
$BODY$
DECLARE
  wvp real;
  result real;
BEGIN

  -- Water Vapour Pressure in hPa
  wvp := (relative_humidity / 100.0) * 6.105 * pow(2.718281828, ((17.27 * temperature) / (237.7 + temperature)));

  result := temperature + (0.33 * wvp) - (0.7 * wind_speed) - 4.00;

  return result;
END; $BODY$
LANGUAGE plpgsql IMMUTABLE;
COMMENT ON FUNCTION apparent_temperature(real, real, real) IS 'Calculates the Apparent Temperature using the formula used for the Australian Bureau of Meteorology - http://www.bom.gov.au/info/thermal_stress/';

-- Checks to see if the application is too old for this database
CREATE OR REPLACE FUNCTION version_check(application character varying, major integer, minor integer, revision integer)
  RETURNS boolean AS $BODY$
DECLARE
  app_key_maj character varying;
  app_key_min character varying;
  app_key_rev character varying;
  key_count integer;

  v_maj integer;
  v_min integer;
  v_rev integer;

  compatible boolean := FALSE;
BEGIN
  -- We'll check for application-specific minimum versions first.
  app_key_maj := upper(application) || '_MIN_VER_MAJ';
  app_key_min := upper(application) || '_MIN_VER_MIN';
  app_key_rev := upper(application) || '_MIN_VER_REV';

  select count(*) into key_count
  from db_info
  where k = app_key_maj or k = app_key_min or k = app_key_rev;

  IF (key_count = 3) THEN
    -- Looks like all three app keys are there. We'll use its versions
    select v::integer into v_maj from db_info where k = app_key_maj;
    select v::integer into v_min from db_info where k = app_key_min;
    select v::integer into v_rev from db_info where k = app_key_rev;
  ELSE
    -- No valid application-specific minimum version. Use the global minimum
    -- instead.
    select v::integer into v_maj from db_info where k = 'MIN_VER_MAJ';
    select v::integer into v_min from db_info where k = 'MIN_VER_MIN';
    select v::integer into v_rev from db_info where k = 'MIN_VER_REV';
  END IF;

  -- We've got the minimum versions. Now lets see if the supplied version
  -- number is newer.

  IF (v_maj < major) THEN
    -- Minimum version is an older major release. We're OK.
    compatible := TRUE;
  ELSEIF (v_maj = major) THEN
    -- Major versions match. Need to check the minor version.
    IF (v_min < minor) THEN
      -- Minimum version is an older release. We're OK.
      compatible := TRUE;
    ELSEIF (v_min = minor) THEN
      -- Major and minor versions match. The revision number will decide if
      -- we're compatible.
      if (v_rev <= revision) THEN
        compatible := TRUE;
      END IF;
    END IF;
  END IF;

  return compatible;
END;
$BODY$
LANGUAGE plpgsql;
COMMENT ON FUNCTION version_check(character varying, integer, integer, integer)
IS 'Checks to see if a particular version of an application is too old for this database.';

-- Gets the minimum version of the application needed for this database
CREATE OR REPLACE FUNCTION minimum_version_string(application character varying)
  RETURNS varchar AS $BODY$
DECLARE
  key_maj character varying;
  key_min character varying;
  key_rev character varying;
  key_count integer;

  version_string character varying;
BEGIN
  -- We'll check for application-specific minimum versions first.
  key_maj := upper(application) || '_MIN_VER_MAJ';
  key_min := upper(application) || '_MIN_VER_MIN';
  key_rev := upper(application) || '_MIN_VER_REV';

  select count(*) into key_count
  from db_info
  where k = key_maj or k = key_min or k = key_rev;

  IF (key_count <> 3) THEN
    key_maj := 'MIN_VER_MAJ';
    key_min := 'MIN_VER_MIN';
    key_rev := 'MIN_VER_REV';
  END IF;

  select max(version.major) || '.' || max(version.minor) || '.' || max(version.revision) as version_string
         into version_string
  from (
  select 42 as grp,
         case when k = key_maj then v else NULL end as major,
         case when k = key_min then v else NULL end as minor,
         case when K = key_rev then v else NULL end as revision
  from db_info
  where k = key_maj or k = key_min or k = key_rev
  ) version
  group by version.grp;

  return version_string;
END;
$BODY$
LANGUAGE plpgsql;
COMMENT ON FUNCTION minimum_version_string(character varying)
IS 'Gets minimum version of the application needed by this database.';

-- Takes a compass point (N, NNE, etc) and returns the direction in degrees.
CREATE OR REPLACE FUNCTION wind_direction_to_degrees(wind_direction character varying)
  RETURNS numeric(4,1) AS $BODY$
DECLARE
    degrees numeric(4,1);
BEGIN
    select case
               when wind_direction = 'N' then 0
               when wind_direction = 'NNE' then 22.5
               when wind_direction = 'NE' then 45
               when wind_direction = 'ENE' then 67.5
               when wind_direction = 'E' then 90
               when wind_direction = 'ESE' then 112.5
               when wind_direction = 'SE' then 135
               when wind_direction = 'SSE' then 157.5
               when wind_direction = 'S' then 180
               when wind_direction = 'SSW' then 202.5
               when wind_direction = 'SW' then 225
               when wind_direction = 'WSW' then 247.5
               when wind_direction = 'W' then 270
               when wind_direction = 'WNW' then 292.5
               when wind_direction = 'NW' then 315
               when wind_direction = 'NNW' then 337.5
            end as direction
           into degrees;

    return degrees;
END;
$BODY$
LANGUAGE plpgsql IMMUTABLE;
COMMENT ON FUNCTION wind_direction_to_degrees(character varying) IS
        'Returns the supplied compass point in degrees';

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

        IF(round(NEW.average_wind_speed::numeric, 2) = 0.0) THEN
          NEW.wind_direction = null;
        END IF;

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

-- Computes any computed fields when a new wh1080 sample is inserted (dew point, wind chill, apparent temperature, etc)
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
-- Wind direction should be fine (it'll be null).
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
          where sa.time_stamp = (
            select max(time_stamp)
            from sample ins
            where ins.time_stamp < new_timestamp
                  and ins.station_id = current_station_id
          )
                and sa.station_id = current_station_id;

        update sample
        set rainfall = new_rainfall,
          wind_direction = wind_direction_to_degrees(NEW.wind_direction)
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

        IF(round(NEW.average_wind_speed::numeric, 2) = 0.0) THEN
          NEW.wind_direction = null;
        END IF;

        -- Grab the station code and send out a notification.
        select s.code into station_code
        from station s where s.station_id = NEW.station_id;

        perform pg_notify('live_data_updated', station_code);

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


COMMIT;