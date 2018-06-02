WITH parameters AS (
   SELECT
     :heatBase::float    	              AS heat_base,
     :coolBase::float    	              AS cool_base,
     :start::date                       AS start_date,
     :end::date                         AS end_date,
   	 :fahrenheit::boolean	              AS fahrenheit, -- instead of celsius
   	 :kmh::boolean			                AS kmh,	-- instead of m/s
   	 :mph::boolean			                AS mph,	-- instead of m/s
   	 :inches::boolean		                AS inches, -- instead of mm
     :stationCode::varchar(5)           AS stationCode,
     32::integer                        AS max_high_temp,
     0::integer                         AS max_low_temp,
     0::integer                         AS min_high_temp,
     -18::integer                       AS min_low_temp,
     0.2::float                         AS rain_02,
     2.0::float                         AS rain_2,
     20.0::float                        AS rain_20
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
      (1, :normJan::FLOAT, :normJanRain::FLOAT),
      (2, :normFeb::FLOAT, :normFebRain::FLOAT),
      (3, :normMar::FLOAT, :normMarRain::FLOAT),
      (4, :normApr::FLOAT, :normAprRain::FLOAT),
      (5, :normMay::FLOAT, :normMayRain::FLOAT),
      (6, :normJun::FLOAT, :normJunRain::FLOAT),
      (7, :normJul::FLOAT, :normJulRain::FLOAT),
      (8, :normAug::FLOAT, :normAugRain::FLOAT),
      (9, :normSep::FLOAT, :normSepRain::FLOAT),
      (10, :normOct::FLOAT, :normOctRain::FLOAT),
      (11, :normNov::FLOAT, :normNovRain::FLOAT),
      (12, :normDec::FLOAT , :normDecRain::FLOAT)
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
        s.time_stamp::date                          AS date,
        s.station_id                                AS station_id,
		round(max(temperature)::numeric,1)          AS max_temp,
        round(min(temperature)::numeric,1)          AS min_temp,
        round(avg(temperature)::numeric,1)          AS avg_temp,
        round(sum(rainfall)::numeric,1)             AS tot_rain,
        round(avg(average_wind_speed)::numeric,1)   AS avg_wind,
        round(max(gust_wind_speed)::numeric,1)      AS max_wind,
        sum(cool_degree_days)                       AS tot_cool_degree_days,
        sum(heat_degree_days)                       AS tot_heat_degree_days
      from (
        select
          time_stamp,
          s.station_id,
          temperature,
          rainfall,
          average_wind_speed,
          gust_wind_speed,
          case when temperature > p.cool_base
            then temperature - p.cool_base
          else 0 end * (stn.sample_interval :: NUMERIC / 86400.0) as cool_degree_days,
          case when temperature < p.heat_base
            then p.heat_base - temperature
          else 0 end * (stn.sample_interval :: NUMERIC / 86400.0) as heat_degree_days
        from parameters p, sample s
        inner join station stn on stn.station_id = s.station_id
        where stn.code = p.stationCode
      ) s, parameters
      where s.time_stamp::date between parameters.start_date::date and parameters.end_date::date
      group by s.time_stamp::date, s.station_id
    ) as aggregates, parameters
  group by
    aggregates.date, aggregates.tot_rain, aggregates.max_temp, aggregates.min_temp, aggregates.station_id,
    aggregates.avg_temp, aggregates.avg_wind, aggregates.max_wind, aggregates.tot_cool_degree_days,
    aggregates.tot_heat_degree_days, parameters.rain_02, parameters.rain_2, parameters.rain_20,
    parameters.max_high_temp, parameters.max_low_temp, parameters.min_high_temp, parameters.min_low_temp
),
monthly_aggregates as (
  select
    date_trunc('month', agg.date)::date as date,
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
  inner join normal_readings norm on norm.month = extract(month from agg.date)
  group by date_trunc('month', agg.date)::date, agg.station_id, norm.temp, norm.rain
), yearly_aggregates as (
  select
    agg.station_id                      as station_id,
    date_trunc('year', agg.date)::date  as year,

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
  group by date_trunc('year', agg.date)::date, agg.station_id
),dominant_wind_directions as (
  WITH wind_directions AS (
    SELECT
      station_id,
      date_trunc('year', time_stamp)::date as year,
      wind_direction,
      count(wind_direction) AS count
    FROM sample s, parameters p
    where date_trunc('year', s.time_stamp)::date between p.start_date and p.end_date and s.wind_direction is not null
    GROUP BY date_trunc('year', s.time_stamp) :: DATE, station_id, wind_direction
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
select
  d.year                                                    as year,

  -- Temperature
  lpad(round(d.mean_max::numeric, 1)::varchar, 5, ' ')      as mean_max,
  lpad(round(d.mean_min::numeric, 1)::varchar, 5, ' ')      as mean_min,
  lpad(round(d.mean::numeric, 1)::varchar, 5, ' ')          as mean,
  lpad(round(d.dep_norm_temp::numeric, 1)::varchar, 5, ' ') as dep_norm_temp,
  lpad(round(d.heat_deg_days::numeric, 0)::varchar, 5, ' ') as heat_dd,
  lpad(round(d.cool_deg_days::numeric, 0)::varchar, 5, ' ') as cool_dd,
  lpad(round(d.hi_temp::numeric, 1)::varchar, 5, ' ')       as hi_temp,
  lpad(d.hi_temp_date::varchar, 3, ' ')                     as hi_temp_date,
  lpad(round(d.low::numeric, 1)::varchar, 5, ' ')           as low_temp,
  lpad(d.low_date::varchar, 3, ' ')                         as low_temp_date,
  lpad(d.max_high::varchar, 3, ' ')                         as max_high,
  lpad(d.max_low::varchar, 3, ' ')                          as max_low,
  lpad(d.min_high::varchar, 3, ' ')                         as min_high,
  lpad(d.min_low::varchar, 3, ' ')                          as min_low,

  -- Rain
  lpad(case when p.inches
    then round(d.tot_rain::numeric, 2)
    else round(d.tot_rain::numeric, 1)
    end::varchar, 7, ' ')                                   as tot_rain,
  lpad(case when p.inches
    then round(d.dep_norm_rain::numeric, 2)
    else round(d.dep_norm_rain::numeric, 1)
    end::varchar, 6, ' ')                                   as dep_norm_rain,
  lpad(case when p.inches
    then round(d.max_obs_rain::numeric, 2)
    else round(d.max_obs_rain::numeric, 1)
    end::varchar, 5, ' ')                                   as max_obs_rain,
  lpad(d.max_obs_rain_day::varchar, 3, ' ')                 as max_obs_rain_day,
  lpad(d.rain_02::varchar, 3, ' ')                          as rain_02,
  lpad(d.rain_2::varchar, 3, ' ')                           as rain_2,
  lpad(d.rain_20::varchar, 3, ' ')                          as rain_20,

  -- Wind
  lpad(round(d.avg_wind::numeric, 1)::varchar, 4, ' ')      as avg_wind,
  lpad(round(d.max_wind::numeric, 1)::varchar, 4, ' ')      as hi_wind,
  lpad(d.high_wind_day::varchar, 3, ' ')                    as high_wind_day,
  lpad(d.dom_dir::varchar, 3, ' ')                          as dom_dir

from (  -- This is unit-converted report data, d:
  select
    -- Common Columns
    to_char(data.year, 'YY')                                                              as year,

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
    to_char(data.max_temp_date, 'MON')                                                    as hi_temp_date,
    case when p.fahrenheit then data.min_temp      * 1.8 + 32 else data.min_temp      end as low,
    to_char(data.min_temp_date, 'MON')                                                    as low_date,
    data.max_high_temp                                                                    as max_high,
    data.max_low_temp                                                                     as max_low,
    data.min_high_temp                                                                    as min_high,
    data.min_low_temp                                                                     as min_low,

    -- Rain table
    case when p.inches then data.tot_rain      * 1.0/25.4 else data.tot_rain          end as tot_rain,
    case when p.inches then data.dep_rain_norm * 1.0/25.4 else data.dep_rain_norm     end as dep_norm_rain,
    case when p.inches then data.max_rain      * 1.0/25.4 else data.max_rain          end as max_obs_rain,
    to_char(data.max_rain_date, 'MON')                                                    as max_obs_rain_day,
    data.rain_over_02                                                                     as rain_02,
    data.rain_over_2                                                                      as rain_2,
    data.rain_over_20                                                                     as rain_20,

    -- Wind table
    case when p.kmh then round((data.avg_wind * 3.6)::numeric,1)
         when p.mph then round((data.avg_wind * 2.23694)::numeric,1)
         else            data.avg_wind                                                end as avg_wind,
    case when p.kmh then round((data.max_wind * 3.6)::numeric, 1)
         when p.mph then round((data.max_wind * 2.23694)::numeric, 1)
         else            data.max_wind                                                end as max_wind,
    to_char(data.max_wind_date, 'MON')                                                    as high_wind_day,
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