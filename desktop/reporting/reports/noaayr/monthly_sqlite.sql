WITH parameters AS (
   SELECT
     :heatBase * 1.0                    AS heat_base,
     :coolBase * 1.0                    AS cool_base,
     :start_t                           AS start_date,
     :end_t                             AS end_date,
     :fahrenheit                        AS fahrenheit, -- instead of celsius
     :kmh                               AS kmh, -- instead of m/s
     :mph                               AS mph, -- instead of m/s
     :inches                            AS inches, -- instead of mm
     :stationCode                       AS stationCode,
     32 * 1                             AS max_high_temp,
     0 * 1                              AS max_low_temp,
     0 * 1                              AS min_high_temp,
     -18 * 1                            AS min_low_temp,
     0.2 * 1.0                          AS rain_02,
     2.0 * 1.0                          AS rain_2,
     20.0 * 1.0                         AS rain_20
 ), compass_points AS (
     SELECT 0 AS idx, 'N' AS point union all
     SELECT 1 AS idx, 'NNE' AS point union all
     SELECT 2 AS idx, 'NE' AS point union all
     SELECT 3 AS idx, 'ENE' AS point union all
     SELECT 4 AS idx, 'E' AS point union all
     SELECT 5 AS idx, 'ESE' AS point union all
     SELECT 6 AS idx, 'SE' AS point union all
     SELECT 7 AS idx, 'SSE' AS point union all
     SELECT 8 AS idx, 'S' AS point union all
     SELECT 9 AS idx, 'SSW' AS point union all
     SELECT 10 AS idx, 'SW' AS point union all
     SELECT 11 AS idx, 'WSW' AS point union all
     SELECT 12 AS idx, 'W' AS point union all
     SELECT 13 AS idx, 'WNW' AS point union all
     SELECT 14 AS idx, 'NW' AS point union all
     SELECT 15 AS idx, 'NNW' AS point
), normal_readings as (
    select  1 as month, :normJan * 1.0 as temp, :normJanRain * 1.0 as rain union all
    select  2 as month, :normFeb * 1.0 as temp, :normFebRain * 1.0 as rain union all
    select  3 as month, :normMar * 1.0 as temp, :normMarRain * 1.0 as rain union all
    select  4 as month, :normApr * 1.0 as temp, :normAprRain * 1.0 as rain union all
    select  5 as month, :normMay * 1.0 as temp, :normMayRain * 1.0 as rain union all
    select  6 as month, :normJun * 1.0 as temp, :normJunRain * 1.0 as rain union all
    select  7 as month, :normJul * 1.0 as temp, :normJulRain * 1.0 as rain union all
    select  8 as month, :normAug * 1.0 as temp, :normAugRain * 1.0 as rain union all
    select  9 as month, :normSep * 1.0 as temp, :normSepRain * 1.0 as rain union all
    select 10 as month, :normOct * 1.0 as temp, :normOctRain * 1.0 as rain union all
    select 11 as month, :normNov * 1.0 as temp, :normNovRain * 1.0 as rain union all
    select 12 as month, :normDec * 1.0 as temp, :normDecRain * 1.0 as rain
), daily_aggregates as (
  select
      -- Day info
      aggregates.date                                         AS date,
      aggregates.station_id                                   AS station_id,
      -- Day Counts
      case when aggregates.tot_rain >= parameters.rain_02
        then 1 else 0 end                                     AS rain_over_02,
      case when aggregates.tot_rain >= parameters.rain_2
        then 1 else 0 end                                     AS rain_over_2,
      case when aggregates.tot_rain >= parameters.rain_20
        then 1 else 0 end                                     AS rain_over_20,
      case when aggregates.max_temp >= parameters.max_high_temp
        then 1 else 0 end                                     AS max_high_temp,
      case when aggregates.max_temp <= parameters.max_low_temp
        then 1 else 0 end                                     AS max_low_temp,
      case when aggregates.min_temp <= parameters.min_high_temp
        then 1 else 0 end                                     AS min_high_temp,
      case when aggregates.min_temp <= parameters.min_low_temp
        then 1 else 0 end                                     AS min_low_temp,
      -- day aggregates
      aggregates.max_temp                                     AS max_temp,
      aggregates.min_temp                                     AS min_temp,
      aggregates.avg_temp                                     AS avg_temp,
      aggregates.tot_rain                                     AS tot_rain,
      aggregates.avg_wind                                     AS avg_wind,
      aggregates.max_wind                                     AS max_wind,
      aggregates.tot_cool_degree_days                         AS tot_cool_degree_days,
      aggregates.tot_heat_degree_days                         AS tot_heat_degree_days
    from (
      select   -- Various aggregates per day
        s.time_stamp                       AS date, ------<<<< should this be turns into a date stamp?
        s.station_id                       AS station_id,
        round(max(temperature),1)          AS max_temp,
        round(min(temperature),1)          AS min_temp,
        round(avg(temperature),1)          AS avg_temp,
        round(sum(rainfall),1)             AS tot_rain,
        round(avg(average_wind_speed),1)   AS avg_wind,
        round(max(gust_wind_speed),1)      AS max_wind,
        sum(cool_degree_days)              AS tot_cool_degree_days,
        sum(heat_degree_days)              AS tot_heat_degree_days
      from (
        select
          date(time_stamp, 'unixepoch', 'localtime') as time_stamp,
          s.station_id,
          temperature,
          rainfall,
          average_wind_speed,
          gust_wind_speed,
          case when temperature > p.cool_base
            then temperature - p.cool_base
          else 0 end * ((stn.sample_interval*60) / 86400.0) as cool_degree_days,
          case when temperature < p.heat_base
            then p.heat_base - temperature
          else 0 end * ((stn.sample_interval*60) / 86400.0) as heat_degree_days
        from parameters p, sample s
        inner join station stn on stn.station_id = s.station_id
        where stn.code = p.stationCode
      ) s, parameters
      where s.time_stamp between date(parameters.start_date, 'unixepoch', 'localtime')
                             and date(parameters.end_date, 'unixepoch', 'localtime')
      group by date(s.time_stamp), s.station_id
    ) as aggregates, parameters
  group by
    aggregates.date, aggregates.tot_rain, aggregates.max_temp, aggregates.min_temp, aggregates.station_id,
    aggregates.avg_temp, aggregates.avg_wind, aggregates.max_wind, aggregates.tot_cool_degree_days,
    aggregates.tot_heat_degree_days, parameters.rain_02, parameters.rain_2, parameters.rain_20,
    parameters.max_high_temp, parameters.max_low_temp, parameters.min_high_temp, parameters.min_low_temp
),
monthly_aggregates as (
  select
    --date_trunc('month', agg.date)::date as date,
    strftime('%Y-%m-01 00:00:00', agg.date)
                                        as date,
    agg.station_id as station_id,

    -- Monthly counts
    sum(agg.rain_over_02)               as rain_over_02,
    sum(agg.rain_over_2)                as rain_over_2,
    sum(agg.rain_over_20)               as rain_over_20,
    sum(agg.max_high_temp)              as max_high_temp,
    sum(agg.max_low_temp)               as max_low_temp,
    sum(agg.min_high_temp)              as min_high_temp,
    sum(agg.min_low_temp)               as min_low_temp,

    -- Monthly aggregates - temperature
    avg(agg.max_temp)                   as max_avg_temp,
    avg(agg.min_temp)                   as min_avg_temp,
    avg(agg.avg_temp)                   as avg_temp,

    sum(agg.tot_cool_degree_days)       as tot_cool_degree_days,
    sum(agg.tot_heat_degree_days)       as tot_heat_degree_days,
    max(agg.max_temp)                   as max_temp,
    min(agg.min_temp)                   as min_temp,

    -- Monthly aggregates - rainfall
    sum(agg.tot_rain)                   as tot_rain,

    max(agg.tot_rain)                   as max_rain,

    -- Monthly aggregates - wind
    avg(agg.avg_wind)                   as avg_wind,
    max(agg.max_wind)                   as max_wind

  from daily_aggregates as agg
  group by strftime('%Y-%m-01 00:00:00', agg.date), agg.station_id
), dominant_wind_directions as (
  WITH wind_directions AS (
    SELECT
      station_id,
      strftime('%Y-%m-01 00:00:00', time_stamp, 'unixepoch', 'localtime') as month,
      wind_direction,
      count(wind_direction) AS count
    FROM sample s, parameters p
    where time_stamp between p.start_date and p.end_date and s.wind_direction is not null
    GROUP BY strftime('%Y-%m-01 00:00:00', time_stamp, 'unixepoch', 'localtime'), station_id, wind_direction
  ), max_count AS (
    SELECT
      station_id,
      month,
      max(count) AS max_count
    FROM wind_directions AS counts
    GROUP BY station_id, month
  )
  SELECT
    d.station_id,
    d.month,
    min(wind_direction) AS wind_direction
  FROM wind_directions d
    INNER JOIN max_count mc ON mc.station_id = d.station_id AND mc.month = d.month AND mc.max_count = d.count
  GROUP BY d.station_id, d.month
), range as (
  WITH RECURSIVE generate_series(value) AS (
    SELECT datetime(p.start_date, 'unixepoch', 'localtime') from parameters p
    UNION ALL
    SELECT datetime(value, '+1 month') FROM generate_series, parameters p
     WHERE datetime(value, '+1 month')<=datetime(p.end_date, 'unixepoch', 'localtime')
  )
  SELECT substr(strftime('%Y', generate_series.value), -2) as year,
         strftime('%m', generate_series.value) + 0 as month
    from generate_series
)
select
  r.year                                                    as year,
  --r.month                                                   as month,
  substr('  ' || r.month, -2)                               as month,

  -- Temperature
  substr('     ' || round(d.mean_max, 1), -5)               as mean_max,
  substr('     ' || round(d.mean_min, 1), -5)               as mean_min,
  substr('     ' || round(d.mean, 1), -5)                   as mean,
  substr('     ' || round(d.dep_norm_temp, 1), -5)          as dep_norm_temp,
  substr('     ' || cast(round(d.heat_deg_days, 0) as integer), -5)          as heat_dd,
  substr('     ' || cast(round(d.cool_deg_days, 0) as integer), -5)          as cool_dd,
  substr('     ' || round(d.hi_temp, 1), -5)                as hi_temp,
  substr('     ' || d.hi_temp_date, -2)                     as hi_temp_date,
  substr('     ' || round(d.low, 1), -5)                    as low_temp,
  substr('     ' || d.low_date, -2)                         as low_temp_date,
  substr('     ' || d.max_high, -2)                         as max_high,
  substr('     ' || d.max_low, -2)                          as max_low,
  substr('     ' || d.min_high, -2)                         as min_high,
  substr('     ' || d.min_low, -2)                          as min_low,

  -- Rain
  substr('     ' || case when p.inches
    then cast(round(d.tot_rain, 2) as text)
    else round(d.tot_rain, 1)
    end, -5)                                                as tot_rain,
  substr('     ' || case when p.inches
    then cast(round(d.dep_norm_rain, 2) as text)
    else round(d.dep_norm_rain, 1)
    end, -6)                                                as dep_norm_rain,
  substr('     ' || case when p.inches
    then cast(round(d.max_obs_rain, 2) as text)
    else round(d.max_obs_rain, 1)
    end, -5)                                                as max_obs_rain,
  substr('     ' || d.max_obs_rain_day, -3)                 as max_obs_rain_day,
  substr('     ' || d.rain_02, -3)                          as rain_02,
  substr('     ' || d.rain_2, -3)                           as rain_2,
  substr('     ' || d.rain_20, -3)                          as rain_20,

  -- Wind
  substr('     ' || round(d.avg_wind, 1), -4)               as avg_wind,
  substr('     ' || round(d.max_wind, 1), -4)               as hi_wind,
  substr('     ' || d.high_wind_day, -2)                    as high_wind_day,
  substr('     ' || d.dom_dir, -3)                          as dom_dir

from range r
left outer join (  -- This is unit-converted report data, d:
  select
    -- Common Columns
    substr(strftime('%Y', data.date), -2)                                                 as year,
    strftime('%m', data.date) + 0                                                         as month,

    -- Temperature Table
    case when p.fahrenheit then data.max_avg_temp  * 1.8 + 32 else data.max_avg_temp  end as mean_max,
    case when p.fahrenheit then data.min_avg_temp  * 1.8 + 32 else data.min_avg_temp  end as mean_min,
    case when p.fahrenheit then data.avg_temp      * 1.8 + 32 else data.avg_temp      end as mean,
    case when p.fahrenheit then data.dep_temp_norm * 1.8 + 32 else data.dep_temp_norm end as dep_norm_temp,
    case when p.fahrenheit
      then data.tot_heat_degree_days * 1.8 + 32 else data.tot_heat_degree_days        end as heat_deg_days,
    case when p.fahrenheit
      then data.tot_cool_degree_days * 1.8 + 32 else data.tot_cool_degree_days        end as cool_deg_days,
    case when p.fahrenheit then data.max_temp      * 1.8 + 32 else data.max_temp      end as hi_temp,
    strftime('%d', data.max_temp_date) + 0                                                as hi_temp_date,
    case when p.fahrenheit then data.min_temp      * 1.8 + 32 else data.min_temp      end as low,
    strftime('%d', data.min_temp_date) + 0                                                as low_date,
    data.max_high_temp                                                                    as max_high,
    data.max_low_temp                                                                     as max_low,
    data.min_high_temp                                                                    as min_high,
    data.min_low_temp                                                                     as min_low,

    -- Rain table
    case when p.inches then data.tot_rain      * 1.0/25.4 else data.tot_rain          end as tot_rain,
    case when p.inches then data.dep_rain_norm * 1.0/25.4 else data.dep_rain_norm     end as dep_norm_rain,
    case when p.inches then data.max_rain      * 1.0/25.4 else data.max_rain          end as max_obs_rain,
    strftime('%d', data.max_rain_date) + 0                                                as max_obs_rain_day,
    data.rain_over_02                                                                     as rain_02,
    data.rain_over_2                                                                      as rain_2,
    data.rain_over_20                                                                     as rain_20,

    -- Wind table
    case when p.kmh then round(data.avg_wind * 3.6,1)
         when p.mph then round(data.avg_wind * 2.23694,1)
         else            data.avg_wind                                                end as avg_wind,
    case when p.kmh then round(data.max_wind * 3.6, 1)
         when p.mph then round(data.max_wind * 2.23694,1)
         else            data.max_wind                                                end as max_wind,
    strftime('%d', data.max_wind_date) + 0                                                as high_wind_day,
    data.dom_wind_direction                                                               as dom_dir

  from (
    select
      match.date,
      match.station_id,

      -- Rain counts
      match.rain_over_02,
      match.rain_over_2,
      match.rain_over_20,

      -- Temperature counts
      match.max_high_temp,
      match.max_low_temp,
      match.min_high_temp,
      match.min_low_temp,

      -- Monthly aggregates - temperature
      match.max_avg_temp,
      match.min_avg_temp,
      match.avg_temp,
      match.dep_temp_norm,
      match.tot_cool_degree_days,
      match.tot_heat_degree_days,
      match.max_temp,
      match.min_temp,
      max(match.max_temp_date) as max_temp_date,
      max(match.min_temp_date) as min_temp_date,

      -- Monthly aggregates - rain
      match.tot_rain,
      match.dep_rain_norm,
      match.max_rain,
      max(match.max_rain_date) as max_rain_date,

      -- Monthly aggregates - wind
      match.avg_wind,
      match.max_wind,
      max(match.max_wind_date) as max_wind_date,
      match.dom_wind_direction
    from (
      select
        monthly.date as date,
        monthly.station_id as station_id,

        -- Rain counts
        monthly.rain_over_02 as rain_over_02,
        monthly.rain_over_2 as rain_over_2,
        monthly.rain_over_20 as rain_over_20,

        -- Temperature counts
        monthly.max_high_temp as max_high_temp,
        monthly.max_low_temp as max_low_temp,
        monthly.min_high_temp as min_high_temp,
        monthly.min_low_temp as min_low_temp,

        -- Monthly aggregates - temperature
        monthly.max_avg_temp as max_avg_temp,
        monthly.min_avg_temp as min_avg_temp,
        monthly.avg_temp as avg_temp,
        monthly.avg_temp - norm.temp as dep_temp_norm,
        monthly.tot_cool_degree_days as tot_cool_degree_days,
        monthly.tot_heat_degree_days as tot_heat_degree_days,
        monthly.max_temp as max_temp,
        case when monthly.max_temp = daily.max_temp
          then daily.date else null end as max_temp_date,
        monthly.min_temp as min_temp,
        case when monthly.min_temp = daily.min_temp
          then daily.date else null end as min_temp_date,

        -- Monthly aggregates - rainfall
        monthly.tot_rain as tot_rain,
        monthly.tot_rain - norm.rain as dep_rain_norm,
        monthly.max_rain as max_rain,
        case when monthly.max_rain = daily.tot_rain
          then daily.date else null end as max_rain_date,

        -- Monthly aggregates - wind
        monthly.avg_wind as avg_wind,
        monthly.max_wind as max_wind,
        case when monthly.max_wind = daily.max_wind
          then daily.date else null end as max_wind_date,
        point.point as dom_wind_direction

      from monthly_aggregates monthly
      inner join normal_readings norm on norm.month = cast(strftime('%m', monthly.date) as integer)
      inner join daily_aggregates daily ON
          daily.station_id = monthly.station_id
        and ( monthly.max_temp = daily.max_temp
          or monthly.min_temp = daily.min_temp
          or monthly.max_rain = daily.tot_rain
          or monthly.max_wind = daily.max_wind)
      left outer join dominant_wind_directions dwd on dwd.station_id = monthly.station_id and dwd.month = monthly.date
      LEFT OUTER JOIN compass_points point ON point.idx = (((dwd.wind_direction * 100) + 1125) % 36000) / 2250
    ) as match
    group by
      match.date, match.station_id, match.rain_over_02, match.rain_over_2, match.rain_over_20,
      match.max_high_temp, match.max_low_temp, match.min_high_temp, match.min_low_temp, match.max_avg_temp,
      match.min_avg_temp, match.avg_temp, match.dep_temp_norm, match.tot_cool_degree_days,
      match.tot_heat_degree_days, match.max_temp, match.tot_rain, match.dep_rain_norm, match.max_rain,
      match.avg_wind, match.max_wind, match.dom_wind_direction, match.min_temp
    order by match.date, match.station_id
  ) as data, parameters p
) as d on d.year = r.year and d.month = r.month
cross join parameters p
