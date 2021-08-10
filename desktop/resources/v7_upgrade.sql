alter table data_file add column start_contiguous_to integer;
alter table data_file add column end_contiguous_from integer;
alter table data_file add column start_time integer;
alter table data_file add column end_time integer;

create view data_file_times as
select df.id,
       df.station,
       stn.sample_interval * 60 as sample_interval_seconds,
       df.is_complete,
       df.start_contiguous_to,
       df.end_contiguous_from,
       df.start_time,
       df.end_time,

       -- end times are the very last second in the timespan covered. So the next data file
       -- should start one second after this one ends.
       df.end_time + 1          as next_datafile_start

from data_file df
         inner join station stn on stn.station_id = df.station
where df.start_time is not null
  and df.end_time is not null
  and df.end_time - df.start_time > 86400 -- Don't want one day data files (if there are any)
order by df.start_time;

update db_metadata set v = 7 where k = 'v';
