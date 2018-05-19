WITH parameters AS (
   SELECT
     :heatBase * 1.0    	              AS heat_base,
     :coolBase * 1.0    	              AS cool_base,
     :start_t                           AS start_date,
     :end_t                             AS end_date,
   	 :fahrenheit        	              AS fahrenheit, -- instead of celsius
   	 :kmh         			                AS kmh,	-- instead of m/s
   	 :mph         			                AS mph,	-- instead of m/s
   	 :inches         		                AS inches, -- instead of mm
     :stationCode                       AS stationCode,
     32 * 1                             AS max_high_temp,
     0 * 1                              AS max_low_temp,
     0 * 1                              AS min_high_temp,
     -18 * 1                            AS min_low_temp,
     0.2 * 1.0                          AS rain_02,
     2.0 * 1.0                          AS rain_2,
     20.0 * 1.0                         AS rain_20
 ), compass_points AS (
  SELECT
	column1 AS idx,
	column2 AS point
  FROM (VALUES
    (0, 'N'), (1, 'NNE'), (2, 'NE'), (3, 'ENE'), (4, 'E'), (5, 'ESE'), (6, 'SE'), (7, 'SSE'), (8, 'S'), (9, 'SSW'),
    (10, 'SW'), (11, 'WSW'), (12, 'W'), (13, 'WNW'), (14, 'NW'), (15, 'NNW')) AS t
), normal_readings as (
  select
    column1 as month,
    column2 as temp,
    column3 as rain
  from (values
      (1, :normJan * 1.0, :normJanRain * 1.0),
      (2, :normFeb * 1.0, :normFebRain * 1.0),
      (3, :normMar * 1.0, :normMarRain * 1.0),
      (4, :normApr * 1.0, :normAprRain * 1.0),
      (5, :normMay * 1.0, :normMayRain * 1.0),
      (6, :normJun * 1.0, :normJunRain * 1.0),
      (7, :normJul * 1.0, :normJulRain * 1.0),
      (8, :normAug * 1.0, :normAugRain * 1.0),
      (9, :normSep * 1.0, :normSepRain * 1.0),
      (10, :normOct * 1.0, :normOctRain * 1.0),
      (11, :normNov * 1.0, :normNovRain * 1.0),
      (12, :normDec * 1.0, :normDecRain * 1.0)
    ) as norm
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
        s.time_stamp                       AS date,
        s.station_id                       AS station_id,
        round(max(temperature),1)          AS max_temp,
        round(min(temperature),1)          AS min_temp,
        round(avg(temperature),1)          AS avg_temp,
        round(sum(rainfall),1)             AS tot_rain,
        round(avg(average_wind_speed),1)   AS avg_wind,
        round(max(gust_wind_speed),1)      AS max_wind,
        sum(cool_degree_days)     		   AS tot_cool_degree_days,
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
          else 0.0 end * ((stn.sample_interval*60) / 86400.0) as cool_degree_days,
          case when temperature < p.heat_base
            then p.heat_base - temperature
          else 0.0 end * ((stn.sample_interval*60) / 86400.0) as heat_degree_days
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
    strftime('%Y-%m-01 00:00:00', agg.date)
                                        as date,
    agg.station_id                      as station_id,

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
    avg(agg.avg_temp) - norm.temp       as dep_norm_temp,
    sum(agg.tot_cool_degree_days)       as tot_cool_degree_days,
    sum(agg.tot_heat_degree_days)       as tot_heat_degree_days,
    max(agg.max_temp)                   as max_temp,
    min(agg.min_temp)                   as min_temp,

    -- Monthly aggregates - rainfall
    sum(agg.tot_rain)                   as tot_rain,
    sum(agg.tot_rain) - norm.rain       as dep_norm_rain,
    max(agg.tot_rain)                   as max_rain,

    -- Monthly aggregates - wind
    avg(agg.avg_wind)                   as avg_wind,
    max(agg.max_wind)                   as max_wind

  from daily_aggregates as agg
  inner join normal_readings norm on norm.month = cast(strftime('%m', agg.date) as integer)
  group by strftime('%Y-%m-01 00:00:00', agg.date), agg.station_id, norm.temp, norm.rain
), yearly_aggregates as (
  select
    agg.station_id                      as station_id,
    strftime('%Y-01-01 00:00:00', agg.date)
                                        as year,

    -- Monthly counts
    sum(agg.rain_over_02)               as rain_over_02,
    sum(agg.rain_over_2)                as rain_over_2,
    sum(agg.rain_over_20)               as rain_over_20,
    sum(agg.max_high_temp)              as max_high_temp,
    sum(agg.max_low_temp)               as max_low_temp,
    sum(agg.min_high_temp)              as min_high_temp,
    sum(agg.min_low_temp)               as min_low_temp,

    -- Monthly aggregates - temperature
    avg(agg.max_avg_temp)               as max_avg_temp,
    avg(agg.min_avg_temp)               as min_avg_temp,
    avg(agg.avg_temp)                   as avg_temp,
    avg(agg.dep_norm_temp)              as dep_norm_temp,
    sum(agg.tot_cool_degree_days)       as tot_cool_degree_days,
    sum(agg.tot_heat_degree_days)       as tot_heat_degree_days,
    max(agg.max_temp)                   as max_temp,
    min(agg.min_temp)                   as min_temp,

    -- Monthly aggregates - rainfall
    sum(agg.tot_rain)                   as tot_rain,
    sum(agg.dep_norm_rain)              as dep_norm_rain,
    max(agg.tot_rain)                   as max_rain,

    -- Monthly aggregates - wind
    avg(agg.avg_wind)                   as avg_wind,
    max(agg.max_wind)                   as max_wind

  from monthly_aggregates as agg
  group by strftime('%Y-01-01 00:00:00', agg.date), agg.station_id
),dominant_wind_directions as (
  WITH wind_directions AS (
    SELECT
      station_id,
      strftime('%Y-01-01 00:00:00', s.time_stamp, 'unixepoch', 'localtime') as year,
      wind_direction,
      count(wind_direction) AS count
    FROM sample s, parameters p
      where s.time_stamp between p.start_date and p.end_date and s.wind_direction is not null
    /*where strftime('%Y-01-01 00:00:00', s.time_stamp, 'unixepoch', 'localtime')
          between datetime(p.start_date, 'unixtime', 'localtime')
          and datetime(p.end_date, 'unixtime', 'localtime') and s.wind_direction is not null*/
    GROUP BY strftime('%Y-01-01 00:00:00', s.time_stamp, 'unixepoch', 'localtime'), station_id, wind_direction
  ), max_count AS (
    SELECT
      station_id,
      year,
      max(count) AS max_count
    FROM wind_directions AS counts
    GROUP BY station_id, year
  )
  SELECT
    d.station_id,
    d.year,
    min(wind_direction) AS wind_direction
  FROM wind_directions d
    INNER JOIN max_count mc ON mc.station_id = d.station_id AND mc.year = d.year AND mc.max_count = d.count
  GROUP BY d.station_id, d.year
)

  -- TODO:
  -- heat_dd and cool_dd are way off
  -- rain day counts are a little high (other station being counted?)
select
  d.year                                            as year,

  -- Temperature
  substr('     ' || round(d.mean_max, 1), -5)       as mean_max,
  substr('     ' || round(d.mean_min, 1), -5)       as mean_min,
  substr('     ' || round(d.mean, 1), -5)           as mean,
  substr('     ' || round(d.dep_norm_temp, 1), -5)  as dep_norm_temp,
  substr('     ' || cast(round(d.heat_deg_days, 0) as integer), -5)  as heat_dd,
  substr('     ' || cast(round(d.cool_deg_days, 0) as integer), -5)  as cool_dd,
  substr('     ' || round(d.hi_temp, 1), -5)        as hi_temp,

  substr('     ' || d.hi_temp_date, -3)             as hi_temp_date,
  substr('     ' || round(d.low, 1), -5)            as low_temp,
  substr('     ' || d.low_date, -3)                 as low_temp_date,
  substr('     ' || d.max_high, -3)                 as max_high,
  substr('     ' || d.max_low, -3)                  as max_low,
  substr('     ' || d.min_high, -3)                 as min_high,
  substr('     ' || d.min_low, -3)                  as min_low,

  -- Rain
  substr('       ' || case when p.inches
    then round(d.tot_rain, 2)
    else round(d.tot_rain, 1)
    end, -7)                                        as tot_rain,
  substr('     ' || case when p.inches
    then round(d.dep_norm_rain, 2)
    else round(d.dep_norm_rain, 1)
    end, -5)                                        as dep_norm_rain,
  substr('     ' || case when p.inches
    then round(d.max_obs_rain, 2)
    else round(d.max_obs_rain, 1)
    end, -5)                                        as max_obs_rain,
  substr('     ' || d.max_obs_rain_day, -3)         as max_obs_rain_day,
  substr('     ' || d.rain_02, -3)                  as rain_02,
  substr('     ' || d.rain_2, -3)                   as rain_2,
  substr('     ' || d.rain_20, -3)                  as rain_20,


  -- Wind
  substr('     ' || round(d.avg_wind, 1), -4)        as avg_wind,
  substr('     ' || round(d.max_wind, 1), -4)        as hi_wind,
  substr('     ' || d.high_wind_day, -3)             as high_wind_day,
  substr('     ' || d.dom_dir, -3)                   as dom_dir

from (  -- This is unit-converted report data, d:
  select
    -- Common Columns
    substr(strftime('%Y ', data.year), -3)                                                as year,

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
    case when strftime('%m', data.max_temp_date) = '01' then 'JAN'
         when strftime('%m', data.max_temp_date) = '02' then 'FEB'
         when strftime('%m', data.max_temp_date) = '03' then 'MAR'
         when strftime('%m', data.max_temp_date) = '04' then 'APR'
         when strftime('%m', data.max_temp_date) = '05' then 'MAY'
         when strftime('%m', data.max_temp_date) = '06' then 'JUN'
         when strftime('%m', data.max_temp_date) = '07' then 'JUL'
         when strftime('%m', data.max_temp_date) = '08' then 'AUG'
         when strftime('%m', data.max_temp_date) = '09' then 'SEP'
         when strftime('%m', data.max_temp_date) = '10' then 'OCT'
         when strftime('%m', data.max_temp_date) = '11' then 'NOV'
         when strftime('%m', data.max_temp_date) = '12' then 'DEC'                    end as hi_temp_date,
    case when p.fahrenheit then data.min_temp      * 1.8 + 32 else data.min_temp      end as low,
    case when strftime('%m', data.min_temp_date) = '01' then 'JAN'
         when strftime('%m', data.min_temp_date) = '02' then 'FEB'
         when strftime('%m', data.min_temp_date) = '03' then 'MAR'
         when strftime('%m', data.min_temp_date) = '04' then 'APR'
         when strftime('%m', data.min_temp_date) = '05' then 'MAY'
         when strftime('%m', data.min_temp_date) = '06' then 'JUN'
         when strftime('%m', data.min_temp_date) = '07' then 'JUL'
         when strftime('%m', data.min_temp_date) = '08' then 'AUG'
         when strftime('%m', data.min_temp_date) = '09' then 'SEP'
         when strftime('%m', data.min_temp_date) = '10' then 'OCT'
         when strftime('%m', data.min_temp_date) = '11' then 'NOV'
         when strftime('%m', data.min_temp_date) = '12' then 'DEC'                    end as low_date,
    data.max_high_temp                                                                    as max_high,
    data.max_low_temp                                                                     as max_low,
    data.min_high_temp                                                                    as min_high,
    data.min_low_temp                                                                     as min_low,

    -- Rain table
    case when p.inches then data.tot_rain      * 1.0/25.4 else data.tot_rain          end as tot_rain,
    case when p.inches then data.dep_rain_norm * 1.0/25.4 else data.dep_rain_norm     end as dep_norm_rain,
    case when p.inches then data.max_rain      * 1.0/25.4 else data.max_rain          end as max_obs_rain,
    case when strftime('%m', data.max_rain_date) = '01' then 'JAN'
         when strftime('%m', data.max_rain_date) = '02' then 'FEB'
         when strftime('%m', data.max_rain_date) = '03' then 'MAR'
         when strftime('%m', data.max_rain_date) = '04' then 'APR'
         when strftime('%m', data.max_rain_date) = '05' then 'MAY'
         when strftime('%m', data.max_rain_date) = '06' then 'JUN'
         when strftime('%m', data.max_rain_date) = '07' then 'JUL'
         when strftime('%m', data.max_rain_date) = '08' then 'AUG'
         when strftime('%m', data.max_rain_date) = '09' then 'SEP'
         when strftime('%m', data.max_rain_date) = '10' then 'OCT'
         when strftime('%m', data.max_rain_date) = '11' then 'NOV'
         when strftime('%m', data.max_rain_date) = '12' then 'DEC'                    end as max_obs_rain_day,
    data.rain_over_02                                                                     as rain_02,
    data.rain_over_2                                                                      as rain_2,
    data.rain_over_20                                                                     as rain_20,

    -- Wind table
    case when p.kmh then round(data.avg_wind * 3.6,1)
         when p.mph then round(data.avg_wind * 2.23694,1)
         else            data.avg_wind                                                end as avg_wind,
    case when p.kmh then round(data.max_wind * 3.6,1)
         when p.mph then round(data.max_wind * 2.23694,1)
         else            data.max_wind                                                end as max_wind,
    case when strftime('%m', data.max_wind_date) = '01' then 'JAN'
         when strftime('%m', data.max_wind_date) = '02' then 'FEB'
         when strftime('%m', data.max_wind_date) = '03' then 'MAR'
         when strftime('%m', data.max_wind_date) = '04' then 'APR'
         when strftime('%m', data.max_wind_date) = '05' then 'MAY'
         when strftime('%m', data.max_wind_date) = '06' then 'JUN'
         when strftime('%m', data.max_wind_date) = '07' then 'JUL'
         when strftime('%m', data.max_wind_date) = '08' then 'AUG'
         when strftime('%m', data.max_wind_date) = '09' then 'SEP'
         when strftime('%m', data.max_wind_date) = '10' then 'OCT'
         when strftime('%m', data.max_wind_date) = '11' then 'NOV'
         when strftime('%m', data.max_wind_date) = '12' then 'DEC'                    end as high_wind_day,
    data.dom_wind_direction                                                               as dom_dir

  from (
    select
      match.year,
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
        yearly.year                    as year,
        yearly.station_id              as station_id,

        -- Rain counts
        yearly.rain_over_02            as rain_over_02,
        yearly.rain_over_2             as rain_over_2,
        yearly.rain_over_20            as rain_over_20,

        -- Temperature counts
        yearly.max_high_temp           as max_high_temp,
        yearly.max_low_temp            as max_low_temp,
        yearly.min_high_temp           as min_high_temp,
        yearly.min_low_temp            as min_low_temp,

        -- Monthly aggregates - temperature
        yearly.max_avg_temp            as max_avg_temp,
        yearly.min_avg_temp            as min_avg_temp,
        yearly.avg_temp                as avg_temp,
        yearly.dep_norm_temp           as dep_temp_norm,
        yearly.tot_cool_degree_days    as tot_cool_degree_days,
        yearly.tot_heat_degree_days    as tot_heat_degree_days,
        yearly.max_temp                as max_temp,
        case when yearly.max_temp = monthly.max_temp
          then monthly.date else null end as max_temp_date,
        yearly.min_temp                as min_temp,
        case when yearly.min_temp = monthly.min_temp
          then monthly.date else null end as min_temp_date,

        -- Monthly aggregates - rainfall
        yearly.tot_rain                as tot_rain,
        yearly.dep_norm_rain           as dep_rain_norm,
        yearly.max_rain                as max_rain,
        case when yearly.max_rain = monthly.tot_rain
          then monthly.date else null end as max_rain_date,

        -- Monthly aggregates - wind
        yearly.avg_wind                as avg_wind,
        yearly.max_wind                as max_wind,
        case when yearly.max_wind = monthly.max_wind
          then monthly.date else null end as max_wind_date,
        point.point                     as dom_wind_direction

      from yearly_aggregates yearly
      inner join monthly_aggregates monthly ON
          monthly.station_id = yearly.station_id
          and (yearly.max_temp = monthly.max_temp
               or yearly.min_temp = monthly.min_temp
               or yearly.max_rain = monthly.tot_rain
               or yearly.max_wind = monthly.max_wind)
      left outer join dominant_wind_directions dwd on dwd.station_id = yearly.station_id and dwd.year = yearly.year
      LEFT OUTER JOIN compass_points point ON point.idx = (((dwd.wind_direction * 100) + 1125) % 36000) / 2250
    ) as match
    group by
      match.year, match.station_id, match.rain_over_02, match.rain_over_2, match.rain_over_20,
      match.max_high_temp, match.max_low_temp, match.min_high_temp, match.min_low_temp, match.max_avg_temp,
      match.min_avg_temp, match.avg_temp, match.dep_temp_norm, match.tot_cool_degree_days,
      match.tot_heat_degree_days, match.max_temp, match.tot_rain, match.dep_rain_norm, match.max_rain,
      match.avg_wind, match.max_wind, match.dom_wind_direction, match.min_temp
    order by match.year, match.station_id
  ) as data, parameters p
) as d
cross join parameters p;