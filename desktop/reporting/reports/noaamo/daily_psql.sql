WITH parameters AS (
  SELECT
    :heatBase::float    	AS heat_base,
    :coolBase::float    	AS cool_base,
    :start::date  			AS start_date,
    :end::date  			AS end_date,
	:fahrenheit::boolean	AS fahrenheit, -- instead of celsius
	:kmh::boolean			AS kmh,	-- instead of m/s
	:mph::boolean			AS mph,	-- instead of m/s
	:inches::boolean		AS inches -- instead of mm
), compass_points AS (
  SELECT 
	column1 AS idx, 
	column2 AS point
  FROM (VALUES
    (0, 'N'), (1, 'NNE'), (2, 'NE'), (3, 'ENE'), (4, 'E'), (5, 'ESE'), (6, 'SE'), (7, 'SSE'), (8, 'S'), (9, 'SSW'),
    (10, 'SW'), (11, 'WSW'), (12, 'W'), (13, 'WNW'), (14, 'NW'), (15, 'NNW')) AS t
)
SELECT
  lpad(extract(day from dates.date)::varchar, 2, ' ')					AS day,
  lpad(round((
	case when p.fahrenheit then records.avg_temp * 1.8 + 32
		 else records.avg_temp
	end)::numeric, 1)::varchar, 4, ' ')    								AS mean_temp,
  lpad(round((
	case when p.fahrenheit then records.high_temp * 1.8 + 32
		 else records.high_temp
	end)::numeric, 1)::varchar, 4, ' ')									AS high_temp,
  lpad(to_char(records.high_temp_time, 'fmHH24:MI'), 5, ' ')			AS high_temp_time,
  lpad(round((
	case when p.fahrenheit then records.low_temp * 1.8 + 32
		 else records.low_temp
	end)::numeric, 1)::varchar, 4, ' ')									AS low_temp,
  lpad(to_char(records.low_temp_time, 'fmHH24:MI'), 5, ' ')				AS low_temp_time,
  lpad(round((
	case when p.fahrenheit then records.heat_degree_days * 1.8 + 32
		 else records.heat_degree_days
	end)::numeric, 1)::varchar, 4, ' ')									AS heat_degree_days,
  lpad(round((
	case when p.fahrenheit then records.cool_degree_days * 1.8 + 32
		 else records.cool_degree_days
	end)::numeric, 1)::varchar, 4, ' ')									AS cool_degree_days,
  lpad((
	case when p.inches then round((records.rainfall * 1.0/25.4)::NUMERIC, 2)
		 else round(records.rainfall::NUMERIC, 1)
	end)::varchar, 4, ' ')												AS rain,
  lpad(round(
	case when p.mph then records.avg_wind * 2.23694 
		 when p.kmh then records.avg_wind * 3.6 
		 else records.avg_wind 
	end::NUMERIC, 1)::varchar, 4, ' ')									AS avg_wind_speed,
  lpad(round((
	case when p.mph then records.high_wind * 2.23694 
		 when p.kmh then records.high_wind * 3.6 
		 else records.high_wind 
	end)::NUMERIC, 1)::varchar, 4, ' ')									AS high_wind,
  lpad(to_char(records.high_wind_time, 'fmHH24:MI'), 5, ' ' )			AS high_wind_time,
  lpad(point.point::varchar, 3, ' ')									AS dom_wind_dir
from parameters p, (
  SELECT generate_series(start_date, end_date, '1 day' :: INTERVAL) :: DATE as date, (select station_id from station where lower(code) = lower((:stationCode)::varchar(5))) as station_id
  from parameters
) as dates
LEFT OUTER JOIN (
  SELECT
    data.date_stamp                       AS date,
    data.station_id,
    data.max_gust_wind_speed              AS high_wind,
    max(data.max_gust_wind_speed_ts)      AS high_wind_time,
    data.min_temperature                  AS low_temp,
    max(data.min_temperature_ts)          AS low_temp_time,
    data.max_temperature                  AS high_temp,
    max(data.max_temperature_ts)          AS high_temp_time,
    data.avg_temperature                  AS avg_temp,
    data.avg_wind_speed                   AS avg_wind,
    data.rainfall                         AS rainfall,
    data.heat_degree_days                 AS heat_degree_days,
    data.cool_degree_days                 AS cool_degree_days
  FROM (SELECT
          dr.date_stamp,
          dr.station_id,
          dr.max_gust_wind_speed,
          dr.avg_temperature,
          dr.avg_wind_speed,
          dr.rainfall,
          dr.heat_degree_days,
          dr.cool_degree_days,
          CASE WHEN (s.gust_wind_speed = dr.max_gust_wind_speed)
            THEN s.time_stamp
          ELSE NULL :: TIMESTAMP WITH TIME ZONE END AS max_gust_wind_speed_ts,
          dr.max_temperature,
          CASE WHEN (s.temperature = dr.max_temperature)
            THEN s.time_stamp
          ELSE NULL :: TIMESTAMP WITH TIME ZONE END AS max_temperature_ts,
          dr.min_temperature,
          CASE WHEN (s.temperature = dr.min_temperature)
            THEN s.time_stamp
          ELSE NULL :: TIMESTAMP WITH TIME ZONE END AS min_temperature_ts
        FROM (sample s
          JOIN (SELECT
                  (s.time_stamp) :: DATE                 AS date_stamp,
                  s.station_id,
                  max(s.gust_wind_speed)                 AS max_gust_wind_speed,
                  min(s.temperature)                     AS min_temperature,
                  max(s.temperature)                     AS max_temperature,
                  avg(s.temperature)                     AS avg_temperature,
                  avg(s.average_wind_speed)              AS avg_wind_speed,
                  sum(s.rainfall)                        AS rainfall,
                  sum(s.heat_dd)                         AS heat_degree_days,
                  sum(s.cool_dd)                         AS cool_degree_days
                FROM (
                  SELECT
                    x.time_stamp,
                    x.station_id,
                    x.temperature,
                    x.average_wind_speed,
                    x.gust_wind_speed,
                    x.rainfall,
                    CASE WHEN x.temperature > p.cool_base 
                      THEN x.temperature - p.cool_base    
                    ELSE 0
                    END * (stn.sample_interval :: NUMERIC / 86400.0) AS cool_dd,
                    CASE WHEN x.temperature < p.heat_base 
                      THEN p.heat_base - x.temperature    
                    ELSE 0
                    END * (stn.sample_interval :: NUMERIC / 86400.0) AS heat_dd
                  from parameters p, sample x
                  inner join station stn on stn.station_id = x.station_id
                  where x.time_stamp::date between p.start_date and p.end_date
                ) s, parameters p
                where s.time_stamp::date between p.start_date and p.end_date
                GROUP BY s.time_stamp :: DATE, s.station_id
               ) dr ON (s.time_stamp :: DATE = dr.date_stamp))
        WHERE s.gust_wind_speed = dr.max_gust_wind_speed OR
                ((s.temperature = dr.max_temperature) OR (s.temperature = dr.min_temperature))
       ) data
  GROUP BY data.date_stamp, data.station_id, data.max_gust_wind_speed, data.max_temperature, data.min_temperature,
    data.avg_wind_speed, data.avg_temperature, data.rainfall, data.heat_degree_days, data.cool_degree_days
) as records on records.date = dates.date and records.station_id = dates.station_id
LEFT OUTER JOIN (
    WITH wind_directions AS (
        SELECT
          time_stamp :: DATE    AS date,
          station_id,
          wind_direction,
          count(wind_direction) AS count
        FROM sample s, parameters p
        where s.time_stamp::date between p.start_date and p.end_date and s.wind_direction is not null
        GROUP BY time_stamp :: DATE, station_id, wind_direction
    ),
        max_count AS (
          SELECT
            date,
            station_id,
            max(count) AS max_count
          FROM wind_directions AS counts
          GROUP BY date, station_id
      )
    SELECT
      d.date,
      d.station_id,
      min(wind_direction) AS wind_direction
    FROM wind_directions d
      INNER JOIN max_count mc ON mc.date = d.date AND mc.station_id = d.station_id AND mc.max_count = d.count
    GROUP BY d.date, d.station_id
    ) as dwd on dwd.station_id = records.station_id and dwd.date = records.date
LEFT OUTER JOIN compass_points point ON point.idx = (((dwd.wind_direction * 100) + 1125) % 36000) / 2250
order by dates.date asc
;