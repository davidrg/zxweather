-- Target database: V8
-- Upgrades to: V9
--
-- Adds support for mean sea level pressure and the API level
-- being used by the weather station on last contact.

-- Wipe these tables so that MSL data will be populated where its
-- available.
delete from sample;
delete from data_file;

-- Add the new MSL pressure column
alter table sample add column mean_sea_level_pressure real;

-- And also a column for tracking the API level
alter table station add column api_level integer;

-- Now at verison 9.
update db_metadata set v = 9 where k = 'v';
