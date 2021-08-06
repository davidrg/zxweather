-- For archiving stations:
alter table station add column archived boolean default false;
alter table station add column archived_time integer;
alter table station add column archived_message text;

-- For tracking permanent gaps in datasets:
create table sample_gap (
    sample_gap_id integer not null primary key,
    station_id integer not null references station(station_id),
    start_time integer not null,
    end_time integer not null,
    missing_sample_count integer not null,
    label text
);

-- Gaps must have a unique timespan for a given station.
-- Ideally we'd forbid overlapping gaps here too but we can't do
-- that in SQLite.
create unique index if not exists idx_sample_gap_timespan
    on sample_gap(station_id, start_time, end_time);

update db_metadata set v = 8 where k = 'v';
