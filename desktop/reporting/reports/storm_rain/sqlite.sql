-- This needs the following index for anything approaching reasonable performance:
-- create index idx_rain_time_station on sample(station_id, time_stamp asc, rainfall asc);

-- This query should find all storms along with their total rainfall.
-- Storms with less than 0.4mm of rain are ignored as in all cases these
-- are a single click of the rain gauge with 24 hours of no rain to either
-- side
with marked_storm_rows as (
  select
    s.time_stamp                                     as this_time_stamp,
    s.station_id,

    -- A storm ends when:
    case when nc.next is null
      then 1 -- There is no newer data (storm in progress effectively)
    when nc.next - s.time_stamp > 86400
      then 1 -- Next row with rain is more than 24h in the future
    else 0 end                                       as end_of_storm,
    -- otherwise this row is not the end of a storm

    -- A storm potentially starts when
    case when cp.prev is null
      then 1 -- There is no older data (storm in progress effectively)
    when s.time_stamp - cp.prev > 86400
      then 1 -- Next next oldest row with rain is more than 24h ago
    else 0 end                                       as start_of_storm -- otherwise this row can not be the start of a storm
  -- For a storm to actually start we need 0.4m of rain or greater.
  -- WeatherLink doesn't seem to care if this is all in one sample or not so we'll just filter out
  -- storms with less than 0.4mm of rain which should have the same effect as checking how much
  -- rain surrounds a potential storm start while being somewhat cheaper to compute.
  from sample s
    left outer join ( -- Next is the next newest record (next record in the future)
                      select
                        cur.sample_id   as sample_id,
                        cur.station_id  as station_id,
                        next.time_stamp as next,
                        cur.time_stamp  as cur
                      from sample cur, sample next
                      where cur.station_id = next.station_id
                        and next.time_stamp = (
                        select min(time_stamp)
                        from sample x
                        where x.time_stamp > cur.time_stamp
                              and x.station_id = cur.station_id
                              and x.rainfall > 0)
                            and cur.rainfall > 0
                            and next.rainfall > 0
                    ) nc on nc.station_id = s.station_id and nc.cur = s.time_stamp
    left outer join ( -- prev is the next oldest record (next record in the past)
                      select
                        cur.sample_id   as sample_id,
                        cur.station_id  as station_id,
                        cur.time_stamp  as cur,
                        prev.time_stamp as prev
                      from sample cur, sample prev
                      where cur.station_id = prev.station_id
                        and prev.time_stamp = (
                        select max(time_stamp)
                        from sample x
                        where x.time_stamp < cur.time_stamp
                              and x.station_id = cur.station_id
                              and x.rainfall > 0)
                            and cur.rainfall > 0
                            and prev.rainfall > 0
                    ) cp on cp.station_id = s.station_id and cp.cur = s.time_stamp
  where s.rainfall > 0
), storm_terminators as (
  select
    this_time_stamp as time_stamp,
    start_of_storm  as start_of_storm,
    end_of_storm    as end_of_storm,
    station_id      as station_id
  from marked_storm_rows where start_of_storm = 1 or end_of_storm = 1
), storms as (
    select
      start.storm_start_time as storm_start_time,
      cur.time_stamp         as storm_end_time,
      cur.station_id         as station_id,
      cur.start_of_storm,
      cur.end_of_storm
    from storm_terminators cur
      inner join (
                   select
                     this.time_stamp  as this,
                     start.time_stamp as storm_start_time,
                     this.station_id  as station_id
                   from storm_terminators this, storm_terminators start
                   where this.station_id = start.station_id and start.time_stamp = (
                     select max(time_stamp)
                     from storm_terminators
                     where time_stamp <= this.time_stamp
                           and start_of_storm = 1
                           and station_id = this.station_id
                   )
                 ) as start on start.this = cur.time_stamp and start.station_id = cur.station_id
    where cur.end_of_storm = 1
)

select
  datetime(storm.storm_start_time, 'unixepoch', 'localtime') as start_time,
  datetime(storm.storm_end_time, 'unixepoch', 'localtime') as end_time,
  case
  when ((storm.storm_end_time - storm.storm_start_time) / 86400) > 0
    then ((storm.storm_end_time - storm.storm_start_time) / 86400) || ' days ' || strftime('%H:%M:%S', (storm.storm_end_time - storm.storm_start_time) % 86400, 'unixepoch')
    else strftime('%H:%M:%S', (storm.storm_end_time - storm.storm_start_time) % 86400, 'unixepoch')
  end as duration,
  cast(round(sum(s.rainfall),1) as text) as total_rainfall
from storms storm
inner join sample s on s.time_stamp between storm.storm_start_time and storm.storm_end_time
  and s.station_id = storm.station_id
inner join station stn on stn.station_id = storm.station_id
  where stn.code = :stationCode
  and storm.storm_start_time between :start_t and :end_t
group by storm.storm_start_time, storm.storm_end_time, storm.station_id
having sum(s.rainfall) > 0.2 -- Filter out storms smaller than 0.4mm - isolated rain clicks don't count
order by sum(s.rainfall) desc;