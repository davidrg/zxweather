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
INSERT INTO station_type(code, title) VALUES('DAVIS', 'Davis Vantage Pro2 or Vantage Vue');

-- Bump database revision
update db_info set v = '3' where k = 'DB_VERSION';

-- Versions of the admin tool before v1.0.0 don't know how to initially populate
-- the davis_live_data table when creating a new DAVIS-type station. This will
-- prevent older versions of the admin tool from being used with the database.
insert into db_info(k,v) values('ADMIN_TOOL_MIN_VER_MAJ','1');
insert into db_info(k,v) values('ADMIN_TOOL_MIN_VER_MIN','0');
insert into db_info(k,v) values('ADMIN_TOOL_MIN_VER_REV','0');

-- Versions of the Desktop Client before v1.0.0 don't properly support multiple
-- weather stations in one database due to a few bugs. Sadly, earlier versions
-- don't actually check for version blacklisting so this doesn't really
-- achieve much.
insert into db_info(k,v) values('DESKTOP_MIN_VER_MAJ','1');
insert into db_info(k,v) values('DESKTOP_MIN_VER_MIN','0');
insert into db_info(k,v) values('DESKTOP_MIN_VER_REV','0');

-- Note: The WH1080 tools are a bit of a special case as they're currently
-- versioned separately from zxweather. Releases at this time are:
--   v2.0.0  - shipped with zxweather v0.2.x. Only checks minimum zxweather
--             version (doesn't supply an app name) so only way to blacklist it
--             is to blacklist all of zxweather v0.2.0
--   v2.0.1  - minor bugfix release shipping with zxweather 1.0.0. Checks full
--             application version (wh1080 v2.0.1).
-- Both versions are fully compatible with zxweather v0.2.0 and v1.0.0.
insert into db_info(k,v) values('WH1080_MIN_VER_MAJ', '2');
insert into db_info(k,v) values('WH1080_MIN_VER_MIN', '0');
insert into db_info(k,v) values('WH1080_MIN_VER_REV', '0');

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

alter table sample add column mean_sea_level_pressure real;
COMMENT ON COLUMN sample.mean_sea_level_pressure IS 'Mean sea level pressure in hPa';

alter table live_data add column mean_sea_level_pressure real;
COMMENT ON COLUMN live_data.mean_sea_level_pressure IS 'Mean sea level pressure in hPa';

-- New sort order column on station table.
alter table station add column sort_order integer;
comment on column station.sort_order is 'The order in which stations should be presented to the user';

-- New columns for station messages
alter table station add column message character varying;
comment on column station.message is 'A message which should be displayed where ever data for the station appears.';

-- Timestamp of the station message
alter table station add column message_timestamp timestamptz;
comment on column station.message_timestamp is 'When the station message was last updated';

alter table station add column station_config character varying;
comment on column station.station_config is 'JSON document containing extra configuration data for the station. The structure of this document depends on the station type.';

alter table station add column site_title character varying;
comment on column station.site_title is 'Title for weather station. Displayed at the top of pages in the web UI.';

-- Add columns for storing station coordinates
alter table station add column latitude real;
comment on column station.latitude is 'Coordinates - latitude';

alter table station add column longitude real;
comment on column station.longitude is 'Coordinates - longitude';

alter table station add column altitude real not null default 0;
comment on column station.altitude is 'Barometer/pressure sensor altitude. Used for relative pressure calculation.';

alter table station add column archived boolean not null default false;
comment on column station.archived is 'If the station is archived. This means its data will never be updated again and there will never be live data for it.';

alter table station add column archived_message character varying;
comment on column station.archived_message is 'Description of why the station was archived. May be publicly visible in the desktop client.';

alter table station add column archived_time timestamptz;
comment on column station.archived_time is 'Date the station was archived.';

alter table station add CONSTRAINT chk_archived
    CHECK ((not archived and archived_time is null and archived_message is null)
        or (archived and archived_time is not null));

-- Adjust comment for wind direction.
COMMENT ON COLUMN sample.wind_direction IS 'Prevailing wind direction in degrees.';
COMMENT ON COLUMN sample.rainfall IS 'Rainfall in mm.';

-- Index over station ID and timestamp to improve performance of chart queries.
CREATE INDEX idx_station_timestamp
  ON sample
  USING btree
  (station_id, time_stamp DESC);
COMMENT ON INDEX idx_station_timestamp
  IS 'Index over station ID and timestamp for finding recent samples';


-- For replicating data to other databases
create table remote_site (
  site_id serial not null primary key,
  hostname character varying
);

comment on table remote_site is 'A remote site data is replicated to';
comment on column remote_site.site_id is 'Site ID, primary key';
comment on column remote_site.hostname is 'Hostname/domain name for the remote site';

create type sample_replication_status as enum('pending', 'awaiting_confirmation', 'done');
comment on type sample_replication_status is 'Sample replication status';

create table replication_status (
  sample_id integer not null references sample(sample_id),
  site_id integer not null references remote_site(site_id),
  status sample_replication_status not null default 'pending',
  status_time timestamptz default NOW(),
  retries integer not null default 0,
  primary key(sample_id, site_id)
);

comment on table replication_status is 'Replication status for each sample';
comment on column replication_status.sample_id is 'ID of the sample';
comment on column replication_status.site_id is 'Remote site';
comment on column replication_status.status is 'Status of the sample on the remote site';
comment on column replication_status.status_time is 'Time the status last changed';
comment on column replication_status.retries is 'Number of times the sample has been transmitted';

CREATE INDEX replication_status_status_site_id_idx
  ON replication_status
  USING btree
  (status, site_id);

-- A table for tracking types of images (eg, a Camera image, a Weather Satellite
-- image, a Weather Satellite sound recording, etc)
create table image_type (
  image_type_id serial primary key not null,
  code varchar(5) unique not null,
  type_name character varying
);
comment on table image_type is 'A type of image stored against a weather station';
comment on column image_type.image_type_id is 'Primary key';
comment on column image_type.code is 'Unique code for image type';
comment on column image_type.type_name is 'Descriptive name of the image type';

-- Image (and video/audio) Types --
-- Camera pictures and time-lapse videos
insert into image_type(code, type_name) values('CAM', 'Camera');
insert into image_type(code, type_name) values('TLVID', 'Time-lapse video');

-- Misc
insert into image_type(code, type_name) values('SPEC', 'Spectrogram');

-- APT broadcasts
insert into image_type(code, type_name) values('APT', 'APT Recording');
insert into image_type(code, type_name) values('APTD', 'APT Unenhanced');
insert into image_type(code, type_name) values('APTN', 'APT Normal');
insert into image_type(code, type_name) values('AEZA', 'APT ZA Enhancement');
insert into image_type(code, type_name) values('AEMB', 'APT MB Enhancement');
insert into image_type(code, type_name) values('AEMD', 'APT MD Enhancement');
insert into image_type(code, type_name) values('AEBD', 'APT BD Enhancement');
insert into image_type(code, type_name) values('AECC', 'APT CC Enhancement');
insert into image_type(code, type_name) values('AEEC', 'APT EC Enhancement');
insert into image_type(code, type_name) values('AEHE', 'APT HE Enhancement');
insert into image_type(code, type_name) values('AEHF', 'APT HF Enhancement');
insert into image_type(code, type_name) values('AEJF', 'APT JF Enhancement');
insert into image_type(code, type_name) values('AEJJ', 'APT JJ Enhancement');
insert into image_type(code, type_name) values('AETA', 'APT TA Enhancement');
insert into image_type(code, type_name) values('AENO', 'APT NO Enhancement');
insert into image_type(code, type_name) values('AEMCI', 'APT MCIR Enhancement');
insert into image_type(code, type_name) values('AEMSA', 'APT MSA Enhancement');
insert into image_type(code, type_name) values('AEMSP', 'APT MSA-precip Enhancement');
insert into image_type(code, type_name) values('AEMAA', 'APT MSA-anaglyph Enhancement');
insert into image_type(code, type_name) values('AEHVC', 'APT HVC Enhancement');
insert into image_type(code, type_name) values('AEHVT', 'APT HVCT Enhancement');
insert into image_type(code, type_name) values('AEHVP', 'APT HVCT-precip Enhancement');
insert into image_type(code, type_name) values('AEHCP', 'APT HVC-precip Enhancement');
insert into image_type(code, type_name) values('AESEA', 'APT sea Enhancement');
insert into image_type(code, type_name) values('AETHE', 'APT therm Enhancement');
insert into image_type(code, type_name) values('AEVEG', 'APT veg Enhancement');
insert into image_type(code, type_name) values('AEANA', 'APT anaglyph Enhancement');
insert into image_type(code, type_name) values('AECAN', 'APT canaglyph Enhancement');
insert into image_type(code, type_name) values('AECLS', 'APT class Enhancement');
insert into image_type(code, type_name) values('AEHIS', 'APT histeq Enhancement');
insert into image_type(code, type_name) values('AECNT', 'APT contrast Enhancement');
insert into image_type(code, type_name) values('AEINV', 'APT invert Enhancement');
insert into image_type(code, type_name) values('AEBW', 'APT bw Enhancement');


-- Where an image came from (a particular camera, a weather satellite receiving
-- station, etc)
create table image_source (
  image_source_id serial primary key not null,
  code varchar(5) unique not null,
  station_id integer not null references station(station_id),
  source_name character varying,
  description character varying
);

comment on table image_source is 'A camera or other source of images (or image like things)';
comment on column image_source.image_source_id is 'Primary key';
comment on column image_source.code is 'Unique code for the image source';
comment on column image_source.station_id is 'Station the source is associated with';
comment on column image_source.source_name is 'Name of the source';
comment on column image_source.description is 'Description of the source';

-- Images.
create table image (
  image_id serial primary key not null,
  image_type_id integer not null references image_type(image_type_id),
  image_source_id integer not null references image_source(image_source_id),
  time_stamp timestamptz not null,
  title character varying,
  description character varying,
  image_data bytea,
  mime_type character varying,
  metadata json
);
comment on table image is 'An image. This could be a picture from a camera, or an image from a weather satellite or even the original sound recording from the weather satellite.';
comment on column image.image_type_id is 'The type of image this is. This will control how its processed and displayed';
comment on column image.image_source_id is 'The source of the image';
comment on column image.time_stamp is 'Time the image was taken';
comment on column image.title is 'A title for the image';
comment on column image.description is 'A description for the image';
comment on column image.image_data is 'The raw image binary data';
comment on column image.mime_type is 'Image MIME type';
comment on column image.metadata is 'Any additional data for this image';

ALTER TABLE image
  ADD CONSTRAINT unq_image_source_timestamp UNIQUE (image_source_id, image_type_id, time_stamp);
COMMENT ON CONSTRAINT unq_image_source_timestamp ON image
  IS 'Each image for an image source must have a unique timestamp and type';


-- A table for tracking the replication status of images
create type image_status as enum('pending', 'awaiting_confirmation', 'done', 'done_resize');
comment on type image_status is 'Image replication status';

create table image_replication_status (
  image_id integer not null references image(image_id),
  site_id integer not null references remote_site(site_id),
  status image_status not null default 'pending',
  status_time timestamptz default NOW(),
  retries integer not null default 0,
  primary key(image_id, site_id)
);

comment on table image_replication_status is 'Replication status for each image';
comment on column image_replication_status.image_id is 'ID of the image';
comment on column image_replication_status.site_id is 'Remote site';
comment on column image_replication_status.status is 'Status of the sample on the remote site';
comment on column image_replication_status.status_time is 'Time the status last changed';
comment on column image_replication_status.retries is 'Number of times the image has been transmitted';

CREATE INDEX image_replication_status_status_site_id_idx
  ON image_replication_status
  USING btree
  (status, site_id);

-- A table for data specific to Davis-compatible hardware.
create table davis_sample (
  sample_id integer not null primary key references sample(sample_id),
  record_time integer not null,
  record_date integer not null,
  high_temperature float,
  low_temperature float,
  high_rain_rate float,
  solar_radiation int,
  wind_sample_count int,
  gust_wind_direction float,
  average_uv_index numeric(3,1),
  evapotranspiration float,
  high_solar_radiation int,
  high_uv_index numeric(3,1),
  forecast_rule_id int,

  -- Extra sensors. To populate all of these you'll need:
  --    1x 6345 Leaf/Soil transmitter setup as a leaf wetness station
  --    1x 6345 Leaf/soil transmitter setup as a soil moisture station
  --    2x 6382 Temperature/humidity stations
  --    1x 6372 Temperature station
  -- With a single Leaf/Soil transmitter the two leaf temperature columns will
  -- be populated with the same values as the first two soil temperature
  -- columns.
  leaf_wetness_1 int,
  leaf_wetness_2 int,
  leaf_temperature_1 float,
  leaf_temperature_2 float,
  soil_moisture_1 int,
  soil_moisture_2 int,
  soil_moisture_3 int,
  soil_moisture_4 int,
  soil_temperature_1 float,
  soil_temperature_2 float,
  soil_temperature_3 float,
  soil_temperature_4 float,
  extra_humidity_1 rh_percentage,
  extra_humidity_2 rh_percentage,
  extra_temperature_1 float,
  extra_temperature_2 float,
  extra_temperature_3 float
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
comment on column davis_sample.leaf_wetness_1 is 'First leaf wetness. Range is 0-15 (0=dry, 15=wet)';
comment on column davis_sample.leaf_wetness_2 is 'Second leaf wetness. Range is 0-15 (0=dry, 15=wet)';
comment on column davis_sample.leaf_temperature_1 is 'First leaf temperature';
comment on column davis_sample.leaf_temperature_2 is 'Second leaf temperature';
comment on column davis_sample.soil_moisture_1 is 'First soil moisture sensor in centibars.';
comment on column davis_sample.soil_moisture_2 is 'Second soil moisture sensor in centibars.';
comment on column davis_sample.soil_moisture_3 is 'Third soil moisture sensor in centibars.';
comment on column davis_sample.soil_moisture_4 is 'Fourth soil moisture sensor in centibars.';
comment on column davis_sample.soil_temperature_1 is 'First soil temperature sensor.';
comment on column davis_sample.soil_temperature_2 is 'Second soil temperature sensor.';
comment on column davis_sample.soil_temperature_3 is 'Third soil temperature sensor.';
comment on column davis_sample.soil_temperature_4 is 'Fourth soil temperature sensor.';
comment on column davis_sample.extra_humidity_1 is 'First extra humidity sensor';
comment on column davis_sample.extra_humidity_2 is 'Second extra humidity sensor';
comment on column davis_sample.extra_temperature_1 is 'First extra temperature sensor';
comment on column davis_sample.extra_temperature_2 is 'Second extra temperature';
comment on column davis_sample.extra_temperature_3 is 'Third extra temperature sensor';

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
  uv_index numeric(3,1),
  solar_radiation int,
  altimeter_setting real,

  -- A few computed values that can be a bit of a pain to compute on the fly
  average_wind_speed_2m real,
  average_wind_speed_10m real,
  gust_wind_speed_10m real,
  gust_wind_direction_10m integer,
  heat_index real,
  thsw_index real,

    -- Extra sensors. To populate all of these you'll need:
  --    1x 6345 Leaf/Soil transmitter setup as a leaf wetness station
  --    1x 6345 Leaf/soil transmitter setup as a soil moisture station
  --    2x 6382 Temperature/humidity stations
  --    1x 6372 Temperature station
  -- With a single Leaf/Soil transmitter the two leaf temperature columns will
  -- be populated with the same values as the first two soil temperature
  -- columns.
  leaf_wetness_1 int,
  leaf_wetness_2 int,
  leaf_temperature_1 float,
  leaf_temperature_2 float,
  soil_moisture_1 int,
  soil_moisture_2 int,
  soil_moisture_3 int,
  soil_moisture_4 int,
  soil_temperature_1 float,
  soil_temperature_2 float,
  soil_temperature_3 float,
  soil_temperature_4 float,
  extra_humidity_1 rh_percentage,
  extra_humidity_2 rh_percentage,
  extra_temperature_1 float,
  extra_temperature_2 float,
  extra_temperature_3 float
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
comment on column davis_live_data.altimeter_setting is 'Altimeter setting in hPa';
comment on column davis_live_data.average_wind_speed_2m is 'Average wind speed over the last 2 minutes in meters per second';
comment on column davis_live_data.average_wind_speed_10m is 'Average wind speed over the last 10 minutes in meters per second';
comment on column davis_live_data.gust_wind_speed_10m is 'Maximum wind gust in the last 10 minutes in meters per second';
comment on column davis_live_data.gust_wind_direction_10m is 'Direction of maximum wind gust in the last 10 minutes';
comment on column davis_live_data.heat_index is 'Heat index in degrees celsius';
comment on column davis_live_data.thsw_index is 'Temperature-Humidity-Sun-Wind index in degrees celsius';
comment on column davis_live_data.leaf_wetness_1 is 'First leaf wetness. Range is 0-15 (0=dry, 15=wet)';
comment on column davis_live_data.leaf_wetness_2 is 'Second leaf wetness. Range is 0-15 (0=dry, 15=wet)';
comment on column davis_live_data.leaf_temperature_1 is 'First leaf temperature';
comment on column davis_live_data.leaf_temperature_2 is 'Second leaf temperature';
comment on column davis_live_data.soil_moisture_1 is 'First soil moisture sensor in centibars.';
comment on column davis_live_data.soil_moisture_2 is 'Second soil moisture sensor in centibars.';
comment on column davis_live_data.soil_moisture_3 is 'Third soil moisture sensor in centibars.';
comment on column davis_live_data.soil_moisture_4 is 'Fourth soil moisture sensor in centibars.';
comment on column davis_live_data.soil_temperature_1 is 'First soil temperature sensor.';
comment on column davis_live_data.soil_temperature_2 is 'Second soil temperature sensor.';
comment on column davis_live_data.soil_temperature_3 is 'Third soil temperature sensor.';
comment on column davis_live_data.soil_temperature_4 is 'Fourth soil temperature sensor.';
comment on column davis_live_data.extra_humidity_1 is 'First extra humidity sensor';
comment on column davis_live_data.extra_humidity_2 is 'Second extra humidity sensor';
comment on column davis_live_data.extra_temperature_1 is 'First extra temperature sensor';
comment on column davis_live_data.extra_temperature_2 is 'Second extra temperature';
comment on column davis_live_data.extra_temperature_3 is 'Third extra temperature sensor';


create table sample_gap (
    sample_gap_id serial not null primary key,
    station_id integer not null references station(station_id),
    start_time timestamptz not null,
    end_time timestamptz not null,
    missing_sample_count integer not null,
    label character varying
);

comment on table sample_gap is 'Documents known permanent gaps in the sample table. This allows clients that may cache data to know that the gap is to be expected and its ok to cache its existence forever.';
comment on column sample_gap.station_id is 'Station the gap is for';
comment on column sample_gap.start_time is 'The timestamp of the sample directly before the gap. The first missing sample will be start_time + sample_interval';
comment on column sample_gap.end_time is 'The timestamp of the sample directly after the gap. The last missing sample will be end_time - sample_interval';
comment on column sample_gap.missing_sample_count is 'The number of samples that are missing';
comment on column sample_gap.label is 'Optional label/description for the gap - why it exists, etc.';

----------------------------------------------------------------------
-- CONSTRAINTS -------------------------------------------------------
----------------------------------------------------------------------

-- Don't allow duplicate timestamps within a station.
ALTER TABLE sample
  ADD CONSTRAINT station_timestamp_unique UNIQUE(time_stamp, station_id)
  DEFERRABLE INITIALLY IMMEDIATE;
COMMENT ON CONSTRAINT station_timestamp_unique ON sample IS 'Each station can only have one sample for a given timestamp.';

----------------------------------------------------------------------
-- FUNCTIONS ---------------------------------------------------------
----------------------------------------------------------------------

CREATE OR REPLACE FUNCTION add_replication_status()
  RETURNS trigger AS
  $BODY$
DECLARE

BEGIN
    -- If its an insert then add a new replication status record for each remote
    -- site
    IF(TG_OP = 'INSERT') THEN
        insert into replication_status(site_id, sample_id)
        select rs.site_id, NEW.sample_id
        from  remote_site rs
        where rs.site_id not in (
          select rs.site_id
          from remote_site rs
          inner join replication_status as repl on repl.site_id = rs.site_id
                                                   and repl.sample_id = NEW.sample_id
        );
    END IF;

    RETURN NEW;
END;
$BODY$
LANGUAGE plpgsql VOLATILE;
COMMENT ON FUNCTION add_replication_status() IS 'Adds a new replication status record for each remote site for a new sample.';

CREATE TRIGGER add_replication_status AFTER INSERT
   ON sample FOR EACH ROW
   EXECUTE PROCEDURE public.add_replication_status();


CREATE OR REPLACE FUNCTION add_image_replication_status()
  RETURNS trigger AS
  $BODY$
DECLARE

BEGIN
    -- If its an insert then add a new replication status record for each remote
    -- site
    IF(TG_OP = 'INSERT') THEN
        insert into image_replication_status(site_id, image_id)
        select rs.site_id, NEW.image_id
        from  remote_site rs
        where rs.site_id not in (
          select rs.site_id
          from remote_site rs
          inner join image_replication_status as repl on repl.site_id = rs.site_id
                                                     and repl.image_id = NEW.image_id
        );

        perform pg_notify('new_image', NEW.image_id::varchar);
    END IF;

    RETURN NEW;
END;
$BODY$
LANGUAGE plpgsql VOLATILE;
COMMENT ON FUNCTION add_image_replication_status() IS 'Adds a new replication status record for each remote site for a new image.';

CREATE TRIGGER add_image_replication_status AFTER INSERT
   ON image FOR EACH ROW
   EXECUTE PROCEDURE public.add_image_replication_status();


CREATE OR REPLACE FUNCTION compute_sample_values()
  RETURNS trigger AS
  $BODY$
DECLARE
    station_code character varying;
    altitude float;
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
        perform pg_notify('new_sample_id', station_code || ':' || NEW.sample_id::character varying);
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


create or replace function get_all_sample_gaps(character varying)
    returns table (
        station_id integer,
        sample_interval interval,
        gap_start_time timestamptz,
        gap_end_time timestamptz,
        gap_length interval,
        missing_samples integer,
        is_known_gap boolean,
        sample_gap_id integer,
        label character varying) as $$ with ts_calc as (
    select lag(time_stamp) over (order by time_stamp)              as prev_ts,
           time_stamp                                              as row_ts,
           time_stamp - lag(time_stamp) over (order by time_stamp) as delta,
           (stn.sample_interval || ' seconds')::interval           as sample_interval,
           stn.station_id                                          as station_id
    from sample s
             inner join station stn on s.station_id = stn.station_id
    where lower(stn.code) = lower($1)
)
select tsc.station_id                                                   as station_id,
       tsc.sample_interval                                              as sample_interval,
       tsc.prev_ts                                                      as gap_start_time,
       tsc.row_ts                                                       as gap_end_time,
       tsc.delta - tsc.sample_interval                                  as gap_length,
       (extract(epoch from tsc.delta - tsc.sample_interval) / extract(epoch from tsc.sample_interval))::integer
                                                                        as missing_samples,
       case when gap.sample_gap_id is null then false else true end     as is_known_gap,
       gap.sample_gap_id                                                as sample_gap_id,
       gap.label                                                        as label
from ts_calc tsc
         left outer join sample_gap gap on gap.station_id = tsc.station_id
                                           and gap.start_time = tsc.prev_ts
                                           and gap.end_time = tsc.row_ts
where delta >= sample_interval * 2
-- gaps of less than 2 sample interval we'll ignore as they're probably a result of clock adjustments.
$$ language SQL;

comment on function get_all_sample_gaps(character varying) is 'Finds all gaps in the sample table greater than twice the sample interval for the specified station. Result includes if the gap is known (record present in sample_gap table) and its label if it has one. Gaps less than twice the sample interval are ignored as they''re unlikely to be missing data - just a result of things like clock adjustments';

-- Live CSV formats supported for data subscriptions in the weather server
create or replace function live_csv_v1(for_station_id integer)
    returns varchar language plpgsql as $$
declare
    result character varying;
begin
    select coalesce(round(ld.temperature::numeric, 2)::varchar, 'None') || ',' ||
           coalesce(round(ld.dew_point::numeric, 2)::varchar, 'None') || ',' ||
           coalesce(round(ld.apparent_temperature::numeric, 2)::varchar, 'None') || ',' ||
           coalesce(round(ld.wind_chill::numeric, 2)::varchar, 'None') || ',' ||
           coalesce(ld.relative_humidity::varchar, 'None') || ',' ||
           coalesce(round(ld.indoor_temperature::numeric, 2)::varchar, 'None') || ',' ||
           coalesce(ld.indoor_relative_humidity::varchar, 'None') || ',' ||
           coalesce(ld.absolute_pressure::varchar, 'None') || ',' ||
           coalesce(round(ld.average_wind_speed::numeric, 2)::varchar, 'None') || ',' ||
           coalesce(ld.wind_direction::varchar, 'None') ||
           case when st.code = 'DAVIS' then ',' ||
                    coalesce(dd.bar_trend::varchar, 'None') || ',' ||
                    coalesce(dd.rain_rate::varchar, 'None') || ',' ||
                    coalesce(dd.storm_rain::varchar, 'None') || ',' ||
                    coalesce(dd.current_storm_start_date::varchar, 'None') || ',' ||
                    coalesce(dd.transmitter_battery::varchar, 'None') || ',' ||
                    coalesce(round(dd.console_battery_voltage::numeric, 2)::varchar, 'None') ||','||
                    coalesce(dd.forecast_icon::varchar, 'None') || ',' ||
                    coalesce(dd.forecast_rule_id::varchar, 'None') || ',' ||
                    coalesce(dd.uv_index::varchar, 'None') || ',' ||
                    coalesce(dd.solar_radiation::varchar, 'None') || ',' ||
                    coalesce(dd.leaf_wetness_1::varchar, 'None') || ',' ||
                    coalesce(dd.leaf_wetness_2::varchar, 'None')  || ',' ||
                    coalesce(round(dd.leaf_temperature_1::numeric, 2)::varchar, 'None') || ',' ||
                    coalesce(round(dd.leaf_temperature_2::numeric, 2)::varchar, 'None') || ',' ||
                    coalesce(dd.soil_moisture_1::varchar, 'None') || ',' ||
                    coalesce(dd.soil_moisture_2::varchar, 'None') || ',' ||
                    coalesce(dd.soil_moisture_3::varchar, 'None') || ',' ||
                    coalesce(dd.soil_moisture_4::varchar, 'None') || ',' ||
                    coalesce(round(dd.soil_temperature_1::numeric, 2)::varchar, 'None') || ',' ||
                    coalesce(round(dd.soil_temperature_2::numeric, 2)::varchar, 'None') || ',' ||
                    coalesce(round(dd.soil_temperature_3::numeric, 2)::varchar, 'None') || ',' ||
                    coalesce(round(dd.soil_temperature_4::numeric, 2)::varchar, 'None') || ',' ||
                    coalesce(round(dd.extra_temperature_1::numeric, 2)::varchar, 'None') || ',' ||
                    coalesce(round(dd.extra_temperature_2::numeric, 2)::varchar, 'None') || ',' ||
                    coalesce(round(dd.extra_temperature_3::numeric, 2)::varchar, 'None') || ',' ||
                    coalesce(dd.extra_humidity_1::varchar, 'None') || ',' ||
                    coalesce(dd.extra_humidity_2::varchar, 'None')
                else '' end
           into result
    from live_data ld
    inner join station stn on stn.station_id = ld.station_id
    inner join station_type st on stn.station_type_id = st.station_type_id
    left outer join davis_live_data dd on stn.station_id = dd.station_id
    where ld.station_id = for_station_id;

    return result;
    end;
$$;
comment on function live_csv_v1 is 'Gets version 1 of the live CSV format compatible with the desktop client up to September 2021.';


create or replace function live_csv_v2(for_station_id integer)
    returns varchar language plpgsql as $$
declare
    result character varying;
begin
    select to_char(ld.download_timestamp, 'YYYY-MM-DD"T"HH24:MI:SS"Z"') || ',' ||
           coalesce(round(ld.temperature::numeric, 2)::varchar, 'None') || ',' ||
           coalesce(round(ld.dew_point::numeric, 2)::varchar, 'None') || ',' ||
           coalesce(round(ld.apparent_temperature::numeric, 2)::varchar, 'None') || ',' ||
           coalesce(round(ld.wind_chill::numeric, 2)::varchar, 'None') || ',' ||
           coalesce(ld.relative_humidity::varchar, 'None') || ',' ||
           coalesce(round(ld.indoor_temperature::numeric, 2)::varchar, 'None') || ',' ||
           coalesce(ld.indoor_relative_humidity::varchar, 'None') || ',' ||
           coalesce(ld.absolute_pressure::varchar, 'None') || ',' ||
           coalesce(ld.mean_sea_level_pressure::varchar, 'None') || ',' ||
           coalesce(round(ld.average_wind_speed::numeric, 2)::varchar, 'None') || ',' ||
           coalesce(ld.wind_direction::varchar, 'None') ||
           case when st.code = 'DAVIS' then ',' ||
                    coalesce(dd.bar_trend::varchar, 'None') || ',' ||
                    coalesce(dd.rain_rate::varchar, 'None') || ',' ||
                    coalesce(dd.storm_rain::varchar, 'None') || ',' ||
                    coalesce(dd.current_storm_start_date::varchar, 'None') || ',' ||
                    coalesce(dd.transmitter_battery::varchar, 'None') || ',' ||
                    coalesce(round(dd.console_battery_voltage::numeric, 2)::varchar, 'None') ||','||
                    coalesce(dd.forecast_icon::varchar, 'None') || ',' ||
                    coalesce(dd.forecast_rule_id::varchar, 'None') || ',' ||
                    coalesce(dd.uv_index::varchar, 'None') || ',' ||
                    coalesce(dd.solar_radiation::varchar, 'None') || ',' ||
                    coalesce(round(dd.average_wind_speed_2m::numeric, 2)::varchar, 'None') || ',' ||
                    coalesce(round(dd.average_wind_speed_10m::numeric, 2)::varchar, 'None') || ',' ||
                    coalesce(round(dd.gust_wind_speed_10m::numeric, 2)::varchar, 'None') || ',' ||
                    coalesce(dd.gust_wind_direction_10m::varchar, 'None') || ',' ||
                    coalesce(round(dd.heat_index::numeric, 2)::varchar, 'None') || ',' ||
                    coalesce(round(dd.thsw_index::numeric, 2)::varchar, 'None') || ',' ||
                    coalesce(dd.altimeter_setting::varchar, 'None') || ',' ||
                    coalesce(dd.leaf_wetness_1::varchar, 'None') || ',' ||
                    coalesce(dd.leaf_wetness_2::varchar, 'None')  || ',' ||
                    coalesce(round(dd.leaf_temperature_1::numeric, 2)::varchar, 'None') || ',' ||
                    coalesce(round(dd.leaf_temperature_2::numeric, 2)::varchar, 'None') || ',' ||
                    coalesce(dd.soil_moisture_1::varchar, 'None') || ',' ||
                    coalesce(dd.soil_moisture_2::varchar, 'None') || ',' ||
                    coalesce(dd.soil_moisture_3::varchar, 'None') || ',' ||
                    coalesce(dd.soil_moisture_4::varchar, 'None') || ',' ||
                    coalesce(round(dd.soil_temperature_1::numeric, 2)::varchar, 'None') || ',' ||
                    coalesce(round(dd.soil_temperature_2::numeric, 2)::varchar, 'None') || ',' ||
                    coalesce(round(dd.soil_temperature_3::numeric, 2)::varchar, 'None') || ',' ||
                    coalesce(round(dd.soil_temperature_4::numeric, 2)::varchar, 'None') || ',' ||
                    coalesce(round(dd.extra_temperature_1::numeric, 2)::varchar, 'None') || ',' ||
                    coalesce(round(dd.extra_temperature_2::numeric, 2)::varchar, 'None') || ',' ||
                    coalesce(round(dd.extra_temperature_3::numeric, 2)::varchar, 'None') || ',' ||
                    coalesce(dd.extra_humidity_1::varchar, 'None') || ',' ||
                    coalesce(dd.extra_humidity_2::varchar, 'None')
                else '' end
          into result
    from live_data ld
    inner join station stn on stn.station_id = ld.station_id
    inner join station_type st on stn.station_type_id = st.station_type_id
    left outer join davis_live_data dd on stn.station_id = dd.station_id
    where ld.station_id = for_station_id;

    return result;
    end;
$$;
comment on function live_csv_v2 is 'Gets version 2 of the live CSV format which adds a timestamp, sea level pressure and some extra values for Davis weather stations';


create or replace function get_live_text_record(station_id integer, format integer)
    returns varchar language plpgsql as $$
begin
    return case when format = 1 then live_csv_v1(station_id)
                when format = 2 then live_csv_v2(station_id)
                else 'unsupported format - supported formats are: 1, 2'
    end;
end;
$$;
comment on function get_live_text_record is 'Gets current live data as a text string such as CSV.';


-- Sample CSV formats supported for data subscriptions in the weather server
create or replace function sample_csv_v1(for_station_id integer,
        get_sample_id integer, start_time timestamptz, end_time timestamptz)
    returns table(sample_ts timestamptz, sample_data varchar) language plpgsql as $$
begin
    return query
    select time_stamp,
           (to_char(s.time_stamp, 'YYYY-MM-DD"T"HH24:MI:SS"Z"') || ',' ||
           coalesce(round(s.temperature::numeric, 2)::varchar, 'None') || ',' ||
           coalesce(round(s.dew_point::numeric, 2)::varchar, 'None') || ',' ||
           coalesce(round(s.apparent_temperature::numeric, 2)::varchar, 'None')||','||
           coalesce(round(s.wind_chill::numeric, 2)::varchar, 'None') || ',' ||
           coalesce(s.relative_humidity::varchar, 'None') || ',' ||
           coalesce(round(s.indoor_temperature::numeric, 2)::varchar, 'None') ||','||
           coalesce(s.indoor_relative_humidity::varchar, 'None') || ',' ||
           coalesce(s.absolute_pressure::varchar, 'None') || ',' ||
           coalesce(round(s.average_wind_speed::numeric, 2)::varchar, 'None') || ','||
           coalesce(round(s.gust_wind_speed::numeric, 2)::varchar, 'None') || ',' ||
           coalesce(s.wind_direction::varchar, 'None') || ',' ||
           coalesce(s.rainfall::varchar, 'None') ||
           case when st.code = 'DAVIS' then ',' ||
                    round(ds.average_uv_index::numeric, 2)::varchar || ',' ||
                    round(ds.solar_radiation::numeric, 2)::varchar
                else ''
                end)::Varchar
    from sample s
    inner join station stn on stn.station_id = s.station_id
    inner join station_type st on st.station_type_id = stn.station_type_id
    left outer join davis_sample ds on ds.sample_id = s.sample_id
    where s.station_id = for_station_id
      and (get_sample_id is not null or start_time is not null)
      and (get_sample_id is null or s.sample_id = get_sample_id)
      and (start_time is null or (
          s.time_stamp > start_time
          and (end_time is null OR s.time_stamp < end_time)
        ))
    order by s.time_stamp
    ;

    end;
$$;
comment on function sample_csv_v1 is 'Gets version 1 of the sample CSV format compatible with the desktop client up to September 2021.';

create or replace function sample_csv_v2(for_station_id integer,
        get_sample_id integer, start_time timestamptz, end_time timestamptz)
    returns table(sample_ts timestamptz, sample_data varchar) language plpgsql as $$
begin
    return query
    select time_stamp,
           (to_char(s.time_stamp, 'YYYY-MM-DD"T"HH24:MI:SS"Z"') || ',' ||
           coalesce(round(s.temperature::numeric, 2)::varchar, 'None') || ',' ||
           coalesce(round(s.dew_point::numeric, 2)::varchar, 'None') || ',' ||
           coalesce(round(s.apparent_temperature::numeric, 2)::varchar, 'None')||','||
           coalesce(round(s.wind_chill::numeric, 2)::varchar, 'None') || ',' ||
           coalesce(s.relative_humidity::varchar, 'None') || ',' ||
           coalesce(round(s.indoor_temperature::numeric, 2)::varchar, 'None') ||','||
           coalesce(s.indoor_relative_humidity::varchar, 'None') || ',' ||
           coalesce(s.absolute_pressure::varchar, 'None') || ',' ||
            coalesce(s.mean_sea_level_pressure::varchar, 'None') || ',' ||
           coalesce(round(s.average_wind_speed::numeric, 2)::varchar, 'None') || ','||
           coalesce(round(s.gust_wind_speed::numeric, 2)::varchar, 'None') || ',' ||
           coalesce(s.wind_direction::varchar, 'None') || ',' ||
           coalesce(s.rainfall::varchar, 'None') ||
           case when st.code = 'DAVIS' then ',' ||
                    coalesce(round(ds.average_uv_index::numeric, 2)::varchar, 'None') || ',' ||
                    coalesce(round(ds.solar_radiation::numeric, 2)::varchar, 'None') || ',' ||
                    coalesce(round(ds.high_temperature::numeric, 2)::varchar, 'None') || ',' ||
                    coalesce(round(ds.low_temperature::numeric, 2)::varchar, 'None') || ',' ||
                    coalesce(round(ds.high_rain_rate::numeric, 2)::varchar, 'None') || ',' ||
                    coalesce(round(ds.wind_sample_count::numeric, 2)::varchar, 'None') || ',' ||
                    coalesce(round(ds.gust_wind_direction::numeric, 2)::varchar, 'None') || ',' ||
                    coalesce(round(ds.evapotranspiration::numeric, 4)::varchar, 'None') || ',' ||
                    coalesce(round(ds.high_solar_radiation::numeric, 2)::varchar, 'None') || ',' ||
                    coalesce(round(ds.high_uv_index::numeric, 2)::varchar, 'None') || ',' ||
                    coalesce(round(ds.forecast_rule_id::numeric, 2)::varchar, 'None') || ',' ||
                    coalesce(round(ds.leaf_wetness_1::numeric, 2)::varchar, 'None') || ',' ||
                    coalesce(round(ds.leaf_wetness_2::numeric, 2)::varchar, 'None') || ',' ||
                    coalesce(round(ds.leaf_temperature_1::numeric, 2)::varchar, 'None') || ',' ||
                    coalesce(round(ds.leaf_temperature_2::numeric, 2)::varchar, 'None') || ',' ||
                    coalesce(round(ds.soil_moisture_1::numeric, 2)::varchar, 'None') || ',' ||
                    coalesce(round(ds.soil_moisture_2::numeric, 2)::varchar, 'None') || ',' ||
                    coalesce(round(ds.soil_moisture_3::numeric, 2)::varchar, 'None') || ',' ||
                    coalesce(round(ds.soil_moisture_4::numeric, 2)::varchar, 'None') || ',' ||
                    coalesce(round(ds.soil_temperature_1::numeric, 2)::varchar, 'None') || ',' ||
                    coalesce(round(ds.soil_temperature_2::numeric, 2)::varchar, 'None') || ',' ||
                    coalesce(round(ds.soil_temperature_3::numeric, 2)::varchar, 'None') || ',' ||
                    coalesce(round(ds.soil_temperature_4::numeric, 2)::varchar, 'None') || ',' ||
                    coalesce(round(ds.extra_humidity_1::numeric, 2)::varchar, 'None') || ',' ||
                    coalesce(round(ds.extra_humidity_2::numeric, 2)::varchar, 'None') || ',' ||
                    coalesce(round(ds.extra_temperature_1::numeric, 2)::varchar, 'None') || ',' ||
                    coalesce(round(ds.extra_temperature_2::numeric, 2)::varchar, 'None') || ',' ||
                    coalesce(round(ds.extra_temperature_3::numeric, 2)::varchar, 'None')
                else ''
                end)::Varchar
    from sample s
    inner join station stn on stn.station_id = s.station_id
    inner join station_type st on st.station_type_id = stn.station_type_id
    left outer join davis_sample ds on ds.sample_id = s.sample_id
    where s.station_id = for_station_id
      and (get_sample_id is not null or start_time is not null)
      and (get_sample_id is null or s.sample_id = get_sample_id)
      and (start_time is null or (
          s.time_stamp > start_time
          and (end_time is null OR s.time_stamp < end_time)
        ))
    order by s.time_stamp
    ;

    end;
$$;
comment on function sample_csv_v2 is 'Gets version 2 of the sample CSV format which adds mean sea level pressure plus all davis-specific columns.';


create or replace function get_sample_text_record(station_id integer,
        format integer, sample_id integer,
        start_time timestamptz, end_time timestamptz)
    returns table(sample_ts timestamptz, sample_data varchar) language plpgsql as $$
begin
    if format = 1 then
        return query select * from sample_csv_v1(station_id, sample_id, start_time, end_time);
    elsif format = 2 then
        return query select * from sample_csv_v2(station_id, sample_id, start_time, end_time);
    else
        return query select now(), 'unsupported format - supported formats are: 1, 2';
    end if;
end;
$$;
comment on function get_sample_text_record is 'Gets sample data as a text string such as CSV.';

create or replace function month_samples_tsv(for_station_id integer,
                                             month timestamptz,
                                             broadcast_id integer,
                                             version integer,
                                             include_extra_sensors bool)
    returns table
            (
                sample_ts   timestamptz,
                sample_data varchar
            )
    language plpgsql
as
$$
begin
    return query
        -- Column headings
        select date_trunc('month', month) as time_stamp,
                ('# timestamp	temperature	dew point	apparent temperature	'  ||
                'wind chill	relative humidity	'  ||
                case when version = 1 then 'pressure'
                     else 'absolute pressure	mean sea level pressure'
                end || chr(9) ||
                'indoor temperature	indoor relative humidity	rainfall	'  ||
                'average wind speed	gust wind speed	wind direction	' ||
                'uv index	solar radiation	reception	high temp	low temp	' ||
                'high rain rate	gust direction	evapotranspiration	' ||
                'high solar radiation	high uv index	forecast rule id' ||
                case when include_extra_sensors then
                     '	soil moisture 1	soil moisture 2	soil moisture 3	' ||
                     'soil moisture 4	soil temperature 1	' ||
                     'soil temperature 2	soil temperature 3	' ||
                     'soil temperature 4	leaf wetness 1	leaf wetness 2	' ||
                     'leaf temperature 1	leaf temperature 2	' ||
                     'extra humidity 1	extra humidity 2	' ||
                     'extra temperature 1	extra temperature 2	' ||
                     'extra temperature 3'
                     else ''
                end)::varchar
        union all
        -- Row data
        select cur.time_stamp as time_stamp,
               (
                   to_char(cur.time_stamp, 'YYYY-MM-DD HH24:MI:SSOF') || chr(9) ||
                   coalesce(round(cur.temperature::numeric, 2)::varchar, '?') || chr(9) ||
                   coalesce(round(cur.dew_point::numeric, 1)::varchar, '?') || chr(9) ||
                   coalesce(round(cur.apparent_temperature::numeric, 1)::varchar, '?') || chr(9) ||
                   coalesce(round(cur.wind_chill::numeric, 1)::varchar, '?') || chr(9) ||
                   coalesce(cur.relative_humidity::varchar, '?') || chr(9) ||
                   case when version = 1 then coalesce(round(coalesce(cur.mean_sea_level_pressure, cur.absolute_pressure)::numeric, 2)::varchar, '?')::varchar
                        else coalesce(round(cur.absolute_pressure::numeric, 2)::varchar, '?') || chr(9) ||
                             coalesce(round(cur.mean_sea_level_pressure::numeric, 2)::varchar, '?')
                   end || chr(9) ||
                   coalesce(round(cur.indoor_temperature::numeric, 2)::varchar, '?') || chr(9) ||
                   coalesce(cur.indoor_relative_humidity::varchar, '?') || chr(9) ||
                   coalesce(round(cur.rainfall::numeric, 1)::varchar, '?') || chr(9) ||
                   coalesce(round(cur.average_wind_speed::numeric, 2)::varchar, '?') || chr(9) ||
                   coalesce(round(cur.gust_wind_speed::numeric, 2)::varchar, '?') || chr(9) ||
                   coalesce(cur.wind_direction::varchar, '?') || chr(9) ||
                   coalesce(ds.average_uv_index::varchar, '?') || chr(9) ||
                   coalesce(ds.solar_radiation::varchar, '?') || chr(9) ||
                   -- Reception:
                   case when broadcast_id is null then '?'
                        else coalesce(round((ds.wind_sample_count /
                               ((s.sample_interval::float) / ((41 + broadcast_id - 1)::float / 16.0)) * 100)::numeric,
                              1)::varchar, '?')
                   end::varchar || chr(9) ||
                   coalesce(round(ds.high_temperature::numeric, 2)::varchar, '?') || chr(9) ||
                   coalesce(round(ds.low_temperature::numeric, 2)::varchar, '?') || chr(9) ||
                   coalesce(round(ds.high_rain_rate::numeric, 2)::varchar, '?') || chr(9) ||
                   coalesce(ds.gust_wind_direction::varchar, '?') || chr(9) ||
                   coalesce(ds.evapotranspiration::varchar, '?') || chr(9) ||
                   coalesce(ds.high_solar_radiation::varchar, '?') || chr(9) ||
                   coalesce(ds.high_uv_index::varchar, '?') || chr(9) ||
                   coalesce(ds.forecast_rule_id::varchar, '?') ||
                   case when include_extra_sensors then chr(9) ||
                           coalesce(ds.soil_moisture_1::varchar, '?') || chr(9) ||
                           coalesce(ds.soil_moisture_2::varchar, '?') || chr(9) ||
                           coalesce(ds.soil_moisture_3::varchar, '?') || chr(9) ||
                           coalesce(ds.soil_moisture_4::varchar, '?') || chr(9) ||
                           coalesce(round(ds.soil_temperature_1::numeric, 2)::varchar, '?') || chr(9) ||
                           coalesce(round(ds.soil_temperature_2::numeric, 2)::varchar, '?') || chr(9) ||
                           coalesce(round(ds.soil_temperature_3::numeric, 2)::varchar, '?') || chr(9) ||
                           coalesce(round(ds.soil_temperature_4::numeric, 2)::varchar, '?') || chr(9) ||
                           coalesce(ds.leaf_wetness_1::varchar, '?') || chr(9) ||
                           coalesce(ds.leaf_wetness_2::varchar, '?') || chr(9) ||
                           coalesce(round(ds.leaf_temperature_1::numeric, 2) ::varchar, '?') || chr(9) ||
                           coalesce(round(ds.leaf_temperature_2::numeric, 2)::varchar, '?') || chr(9) ||
                           coalesce(ds.extra_humidity_1::varchar, '?') || chr(9) ||
                           coalesce(ds.extra_humidity_2::varchar, '?') || chr(9) ||
                           coalesce(round(ds.extra_temperature_1::numeric, 2)::varchar, '?') || chr(9) ||
                           coalesce(round(ds.extra_temperature_2::numeric, 2)::varchar, '?') || chr(9) ||
                           coalesce(round(ds.extra_temperature_3::numeric, 2) ::varchar, '?')
                        else ''
                   end::varchar

               )::varchar as sample_data

        from sample cur
                 inner join station s on s.station_id = cur.station_id
                 left outer join davis_sample ds on ds.sample_id = cur.sample_id
        where date(date_trunc('month', cur.time_stamp)) = date(date_trunc('month', month))
          and cur.station_id = for_station_id
        order by time_stamp;

end;
$$;
comment on function month_samples_tsv is 'Gets tab-delimited sample data for an entire month. Used by some of the web UIs data endpoints.';

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

drop view if exists daily_records;
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
       data.min_sea_level_pressure,
       max(data.min_sea_level_pressure_ts) as min_sea_level_pressure_ts,
       data.max_sea_level_pressure,
       max(data.max_sea_level_pressure_ts) as max_sea_level_pressure_ts,
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
         dr.max_sea_level_pressure,
         case when s.mean_sea_level_pressure = dr.max_sea_level_pressure then s.time_stamp else NULL end as max_sea_level_pressure_ts,
         dr.min_sea_level_pressure,
         case when s.mean_sea_level_pressure = dr.min_sea_level_pressure then s.time_stamp else NULL end as min_sea_level_pressure_ts,
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
  inner join (
    select time_stamp::date as date_stamp,
           s.station_id,
           sum(coalesce(s.rainfall,0)) as "total_rainfall",
           max(s.gust_wind_speed) as "max_gust_wind_speed",
           max(s.average_wind_speed) as "max_average_wind_speed",
           min(s.absolute_pressure) as "min_absolute_pressure",
           max(s.absolute_pressure) as "max_absolute_pressure",
           min(s.mean_sea_level_pressure) as "min_sea_level_pressure",
           max(s.mean_sea_level_pressure) as "max_sea_level_pressure",
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
  on s.time_stamp::date = dr.date_stamp
     and s.station_id = dr.station_id

  -- Filter the samples down to only those with a maximum or minimum reading
  where ( s.gust_wind_speed = dr.max_gust_wind_speed
          or s.average_wind_speed = dr.max_average_wind_speed
          or s.absolute_pressure in (dr.max_absolute_pressure, dr.min_absolute_pressure)
          or s.mean_sea_level_pressure in (dr.max_sea_level_pressure, dr.min_sea_level_pressure)
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
          data.max_sea_level_pressure,
          data.min_sea_level_pressure,
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
COMMENT ON VIEW daily_records IS 'Minimum and maximum temperature, dew point, wind chill, apparent temperature, gust wind speed, average wind speed, absolute pressure and humidity per day.';

drop view if exists monthly_records;
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
       data.min_sea_level_pressure,
       max(data.min_sea_level_pressure_ts) as min_sea_level_pressure_ts,
       data.max_sea_level_pressure,
       max(data.max_sea_level_pressure_ts) as max_sea_level_pressure_ts,
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
         dr.max_sea_level_pressure,
         case when s.mean_sea_level_pressure = dr.max_sea_level_pressure then s.time_stamp else NULL end as max_sea_level_pressure_ts,
         dr.min_sea_level_pressure,
         case when s.mean_sea_level_pressure = dr.min_sea_level_pressure then s.time_stamp else NULL end as min_sea_level_pressure_ts,
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
  inner join (
    select date_trunc('month', s.time_stamp)::date as date_stamp,
           s.station_id,
           sum(coalesce(s.rainfall,0)) as "total_rainfall",
           max(s.gust_wind_speed) as "max_gust_wind_speed",
           max(s.average_wind_speed) as "max_average_wind_speed",
           min(s.absolute_pressure) as "min_absolute_pressure",
           max(s.absolute_pressure) as "max_absolute_pressure",
           min(s.mean_sea_level_pressure) as "min_sea_level_pressure",
           max(s.mean_sea_level_pressure) as "max_sea_level_pressure",
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
  on date_trunc('month', s.time_stamp)::date = dr.date_stamp
     and s.station_id = dr.station_id

  -- Filter the samples down to only those with a maximum or minimum reading
  where ( s.gust_wind_speed = dr.max_gust_wind_speed
          or s.average_wind_speed = dr.max_average_wind_speed
          or s.absolute_pressure in (dr.max_absolute_pressure, dr.min_absolute_pressure)
          or s.mean_sea_level_pressure in (dr.max_sea_level_pressure, dr.min_sea_level_pressure)
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
        data.min_sea_level_pressure,
        data.max_sea_level_pressure,
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
COMMENT ON VIEW monthly_records IS 'Minimum and maximum values for each month.';

drop view if exists yearly_records;
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
       data.min_sea_level_pressure,
       max(data.min_sea_level_pressure_ts) as min_sea_level_pressure_ts,
       data.max_sea_level_pressure,
       max(data.max_sea_level_pressure_ts) as max_sea_level_pressure_ts,
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
          dr.max_sea_level_pressure,
          case when s.mean_sea_level_pressure = dr.max_sea_level_pressure then s.time_stamp else NULL end as max_sea_level_pressure_ts,
          dr.min_sea_level_pressure,
          case when s.mean_sea_level_pressure = dr.min_sea_level_pressure then s.time_stamp else NULL end as min_sea_level_pressure_ts,
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
                    min(s.mean_sea_level_pressure) as "min_sea_level_pressure",
                    max(s.mean_sea_level_pressure) as "max_sea_level_pressure",
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
     and s.station_id = dr.station_id

-- Filter the samples down to only those with a maximum or minimum reading
where ( s.gust_wind_speed = dr.max_gust_wind_speed
        or s.average_wind_speed = dr.max_average_wind_speed
        or s.absolute_pressure in (dr.max_absolute_pressure, dr.min_absolute_pressure)
        or s.mean_sea_level_pressure in (dr.max_sea_level_pressure, dr.min_sea_level_pressure)
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
        data.max_sea_level_pressure,
        data.min_sea_level_pressure,
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
COMMENT ON VIEW yearly_records IS 'Minimum and maximum records for each year.';
----------------------------------------------------------------------
-- END ---------------------------------------------------------------
----------------------------------------------------------------------

-- Uncomment this COMMIT statement when running the script manually.

--COMMIT;