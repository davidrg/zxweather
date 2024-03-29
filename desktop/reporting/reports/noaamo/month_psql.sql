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
        where lower(stn.code) = lower(p.stationCode)
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
  group by date_trunc('month', agg.date)::date, agg.station_id
), dominant_wind_directions as (
  WITH wind_directions AS (
    SELECT
      station_id,
      date_trunc('month', time_stamp)::date as month,
      wind_direction,
      count(wind_direction) AS count
    FROM sample s, parameters p
    where date_trunc('month', s.time_stamp)::date between p.start_date and p.end_date and s.wind_direction is not null
    GROUP BY date_trunc('month', s.time_stamp) :: DATE, station_id, wind_direction
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
)
select
  -- Temperature
  lpad(round(d.mean::numeric, 1)::varchar, 5, ' ')          as mean,
  lpad(round(d.heat_deg_days::numeric, 1)::varchar, 5, ' ') as heat_dd,
  lpad(round(d.cool_deg_days::numeric, 1)::varchar, 5, ' ') as cool_dd,
  lpad(round(d.hi_temp::numeric, 1)::varchar, 5, ' ')       as hi_temp,
  lpad(d.hi_temp_date::varchar, 2, ' ')                     as hi_temp_date,
  lpad(round(d.low::numeric, 1)::varchar, 5, ' ')           as low_temp,
  lpad(d.low_date::varchar, 2, ' ')                         as low_temp_date,
  lpad(d.max_high::varchar, 2, ' ')                         as max_high,
  lpad(d.max_low::varchar, 2, ' ')                          as max_low,
  lpad(d.min_high::varchar, 2, ' ')                         as min_high,
  lpad(d.min_low::varchar, 2, ' ')                          as min_low,

  -- Rain
  lpad(case when p.inches
    then round(d.tot_rain::numeric, 2)
    else round(d.tot_rain::numeric, 1)
    end::varchar, 5, ' ')                                   as tot_rain,
  lpad(case when p.inches
    then round(d.max_obs_rain::numeric, 2)
    else round(d.max_obs_rain::numeric, 1)
    end::varchar, 5, ' ')                                   as max_obs_rain,
  d.max_obs_rain_day::varchar                               as max_obs_rain_day,
  lpad(d.rain_02::varchar, 2, ' ')                          as rain_02,
  lpad(d.rain_2::varchar, 2, ' ')                           as rain_2,
  lpad(d.rain_20::varchar, 2, ' ')                          as rain_20,

  -- Wind
  lpad(round(d.avg_wind::numeric, 1)::varchar, 4, ' ')      as avg_wind,
  lpad(round(d.max_wind::numeric, 1)::varchar, 4, ' ')      as hi_wind,
  lpad(d.high_wind_day::varchar, 2, ' ')                    as high_wind_day,
  lpad(d.dom_dir::varchar, 3, ' ')                          as dom_dir

from (  -- This is unit-converted report data, d:
  select
    -- Temperature Table
    case when p.fahrenheit then data.avg_temp      * 1.8 + 32 else data.avg_temp      end as mean,
    case when p.fahrenheit
      then data.tot_heat_degree_days * 1.8 + 32 else data.tot_heat_degree_days        end as heat_deg_days,
    case when p.fahrenheit
      then data.tot_cool_degree_days * 1.8 + 32 else data.tot_cool_degree_days        end as cool_deg_days,
    case when p.fahrenheit then data.max_temp      * 1.8 + 32 else data.max_temp      end as hi_temp,
    extract(day from data.max_temp_date)                                                  as hi_temp_date,
    case when p.fahrenheit then data.min_temp      * 1.8 + 32 else data.min_temp      end as low,
    extract(day from data.min_temp_date)                                                  as low_date,
    data.max_high_temp                                                                    as max_high,
    data.max_low_temp                                                                     as max_low,
    data.min_high_temp                                                                    as min_high,
    data.min_low_temp                                                                     as min_low,

    -- Rain table
    case when p.inches then data.tot_rain      * 1.0/25.4 else data.tot_rain          end as tot_rain,
    case when p.inches then data.max_rain      * 1.0/25.4 else data.max_rain          end as max_obs_rain,
    data.max_rain_date::date                                                              as max_obs_rain_day,
    data.rain_over_02                                                                     as rain_02,
    data.rain_over_2                                                                      as rain_2,
    data.rain_over_20                                                                     as rain_20,

    -- Wind table
    case when p.kmh then round((data.avg_wind * 3.6)::numeric,1)
         when p.mph then round((data.avg_wind * 2.23694)::numeric,1)
         else            data.avg_wind                                                end as avg_wind,
    case when p.kmh then round((data.max_wind * 3.6)::numeric,1)
         when p.mph then round((data.max_wind * 2.23694)::numeric,1)
         else            data.max_wind                                                end as max_wind,
    extract(day from data.max_wind_date)                                                  as high_wind_day,
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
      match.avg_temp,
      match.tot_cool_degree_days,
      match.tot_heat_degree_days,
      match.max_temp,
      match.min_temp,
      max(match.max_temp_date) as max_temp_date,
      max(match.min_temp_date) as min_temp_date,

      -- Monthly aggregates - rain
      match.tot_rain,
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
        monthly.avg_temp as avg_temp,
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
      match.max_high_temp, match.max_low_temp, match.min_high_temp, match.min_low_temp,
      match.avg_temp, match.tot_cool_degree_days,
      match.tot_heat_degree_days, match.max_temp, match.tot_rain, match.max_rain,
      match.avg_wind, match.max_wind, match.dom_wind_direction, match.min_temp
    order by match.date, match.station_id
  ) as data, parameters p
) as d
cross join parameters p