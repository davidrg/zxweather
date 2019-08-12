-- Target database: V3
-- Upgrades to: V4
--
-- Adds a table to store known dates with images per data source. This is
-- primarily for the benefit of reports.


create table image_dates (
    image_source_id integer,
    date text
);

-- //////////////////// Update database version //////////////////// --
update db_metadata set v = 4 where k = 'v';
