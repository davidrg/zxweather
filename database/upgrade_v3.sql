----------------------------------------------------------------------------
-- This script is to upgrade from the v0.2 schema.                        --
----------------------------------------------------------------------------
--
-- This script upgrades from the V2 schema used by zxweather v0.2 to the new
-- V3 schema used by zxweather v1.0. The main changes are to do with support
-- for Davis weather stations, specifically the Vantage Vue.

-- If you are running this script manually you should uncomment the following
-- BEGIN statement and the matching COMMIT statement at the end of the file.

--BEGIN;

----------------------------------------------------------------------
-- DATA --------------------------------------------------------------
----------------------------------------------------------------------

-- Add new Davis station type
INSERT INTO station_type(code, title) VALUES('DAVIS', 'Davis Vantage Vue compatible');

-- Bump database revision
update db_info set v = '3' where k = 'DB_VERSION';

-- Versions of the admin tool before v1.0.0 don't know how to initially populate
-- the davis_live_data table when creating a new DAVIS-type station. This will
-- prevent older versions of the admin tool from being used with the database.
insert into db_info(k,v) values('ADMIN_TOOL_MIN_VER_MAJ','1');
insert into db_info(k,v) values('ADMIN_TOOL_MIN_VER_MIN','0');
insert into db_info(k,v) values('ADMIN_TOOL_MIN_VER_REV','0');

----------------------------------------------------------------------
-- TABLES ------------------------------------------------------------
----------------------------------------------------------------------

-- A percentage.
COMMENT ON DOMAIN rh_percentage
  IS 'Relative Humidity percentage (0-100%)';

ALTER TABLE sample
    drop constraint chk_outdoor_relative_humidity;

alter table sample
ADD CONSTRAINT chk_outdoor_relative_humidity
CHECK (relative_humidity > 0 and relative_humidity <= 100);
COMMENT ON CONSTRAINT chk_outdoor_relative_humidity ON sample
IS 'Ensure the outdoor relative humidity is in the range 0-100';

ALTER TABLE sample
    drop constraint chk_indoor_relative_humidity;
ALTER TABLE sample
ADD CONSTRAINT chk_indoor_relative_humidity
CHECK (indoor_relative_humidity > 0 and indoor_relative_humidity <= 100);
COMMENT ON CONSTRAINT chk_indoor_relative_humidity ON sample
IS 'Ensure the indoor relative humidity is in the range 0-100';

-- Timestamp really can't be nullable
alter table sample alter column time_stamp set not null;

-- New sort order column on station table.
alter table station add column sort_order integer;
comment on column station.sort_order is 'The order in which stations should be presented to the user';

-- New columns for station messages
alter table station add column message character varying;
comment on column station.message is 'A message which should be displayed where ever data for the station appears.';

-- Timestamp of the station message
alter table station add column message_timestamp timestamptz;
comment on column station.message_timestamp is 'When the station message was last updated';

-- Adjust comment for wind direction.
COMMENT ON COLUMN sample.wind_direction IS 'Prevailing wind direction in degrees.';

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
  average_uv_index int,
  evapotranspiration float,
  high_solar_radiation float,
  high_uv_index int,
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
  uv_index int,
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
----------------------------------------------------------------------
-- CONSTRAINTS ----------------------------------------------------------
----------------------------------------------------------------------

-- Don't allow duplicate timestamps within a station.
ALTER TABLE sample
  ADD CONSTRAINT station_timestamp_unique UNIQUE(time_stamp, station_id)
  DEFERRABLE INITIALLY IMMEDIATE;
COMMENT ON CONSTRAINT station_timestamp_unique ON sample IS 'Each station can only have one sample for a given timestamp.';

----------------------------------------------------------------------
-- FUNCTIONS ---------------------------------------------------------
----------------------------------------------------------------------

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
-- VIEWS -------------------------------------------------------------
----------------------------------------------------------------------
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
-- END ---------------------------------------------------------------
----------------------------------------------------------------------

-- Uncomment this COMMIT statement when running the script manually.

--COMMIT;