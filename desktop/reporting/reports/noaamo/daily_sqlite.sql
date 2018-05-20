
WITH parameters AS (
  SELECT
    :heatBase    	AS heat_base,
    :coolBase    	AS cool_base,
    :start_t  		AS start_date,
    :end_t  			AS end_date,
	  :fahrenheit	  AS fahrenheit, -- instead of celsius
	  :kmh			    AS kmh,	-- instead of m/s
	  :mph			    AS mph,	-- instead of m/s
	  :inches	      AS inches, -- instead of mm
	  :stationCode as stationCode
), compass_points AS (
  SELECT
	column1 AS idx,
	column2 AS point
  FROM (VALUES
    (0, 'N'), (1, 'NNE'), (2, 'NE'), (3, 'ENE'), (4, 'E'), (5, 'ESE'), (6, 'SE'), (7, 'SSE'), (8, 'S'), (9, 'SSW'),
    (10, 'SW'), (11, 'WSW'), (12, 'W'), (13, 'WNW'), (14, 'NW'), (15, 'NNW')) AS t
), range as (
  WITH RECURSIVE generate_series(value) AS (
    SELECT datetime(p.start_date, 'unixepoch', 'localtime') from parameters p
    UNION ALL
    SELECT datetime(value, '+1 day') FROM generate_series, parameters p
     WHERE datetime(value, '+1 day')<=datetime(p.end_date, 'unixepoch', 'localtime')
  )
  SELECT date(generate_series.value) as date,
        (select station_id from station where lower(code) =
                                              lower(parameters.stationCode)) as station_id
    from generate_series, parameters
)
SELECT
  substr('  ' || cast(strftime('%d', dates.date) + 0 as text), -2)                  as day,
  substr('     ' || round((
	case when p.fahrenheit then records.avg_temp * 1.8 + 32
		 else records.avg_temp
	end), 1), -4)    								                                       AS mean_temp,
  substr('     ' || round((
	case when p.fahrenheit then records.high_temp * 1.8 + 32
		 else records.high_temp
	end), 1), -4)                            AS high_temp,
  
  --substr('     ' || strftime('%H:%M', records.high_temp_time, 'unixepoch', 'localtime'), -5)		AS high_temp_time,
  
  substr('     ' || (strftime('%H', records.high_temp_time, 'unixepoch', 'localtime')  + 0)
				 || strftime(':%M', records.high_temp_time, 'unixepoch', 'localtime'), -5)		AS high_temp_time,
  
  substr('     ' || round((
	case when p.fahrenheit then records.low_temp * 1.8 + 32
		 else records.low_temp
	end), 1), -4)							                                     	AS low_temp,
  
  
  --substr('     ' || strftime('%H:%M',records.low_temp_time, 'unixepoch', 'localtime'), -5)     AS low_temp_time,
  
  substr('     ' || (strftime('%H', records.low_temp_time, 'unixepoch', 'localtime') + 0)
				 || strftime(':%M', records.low_temp_time, 'unixepoch', 'localtime'), -5)		AS low_temp_time,
  
  substr('     ' || round((
	case when p.fahrenheit then records.heat_degree_days * 1.8 + 32
		 else records.heat_degree_days
	end), 1), -4)                          		AS heat_degree_days,
  substr('     ' || round((
	case when p.fahrenheit then records.cool_degree_days * 1.8 + 32
		 else records.cool_degree_days
	end), 1), -4)							                         		AS cool_degree_days,
  substr('     ' || (
	case when p.inches then round((records.rainfall * 1.0/25.4), 2)
		 else round(records.rainfall, 1)
	end), -4)												AS rain,
  substr('     ' || round(
	case when p.mph then records.avg_wind * 2.23694
		 when p.kmh then records.avg_wind * 3.6
		 else records.avg_wind
	end, 1), -4)									AS avg_wind_speed,
  substr('     ' || round((
	case when p.mph then records.high_wind * 2.23694
		 when p.kmh then records.high_wind * 3.6
		 else records.high_wind
	end), 1), -4)									AS high_wind,
  
  
  -- substr('     ' || strftime('%H:%M', records.high_wind_time, 'unixepoch', 'localtime'), -5)			AS high_wind_time,
  
  substr('     ' || (strftime('%H', records.high_wind_time, 'unixepoch', 'localtime') + 0)
				 || strftime(':%M', records.high_wind_time, 'unixepoch', 'localtime'), -5)		AS high_wind_time,
  
  
  substr('     ' || point.point, -3)									AS dom_wind_dir
from parameters p, range as dates
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
          CASE WHEN (s.gust_wind_speed  = dr.max_gust_wind_speed)
            THEN s.time_stamp
          ELSE NULL END AS max_gust_wind_speed_ts,
          dr.max_temperature,
          CASE WHEN (s.temperature = dr.max_temperature)
            THEN s.time_stamp
          ELSE NULL END AS max_temperature_ts,
          dr.min_temperature,
          CASE WHEN (s.temperature = dr.min_temperature)
            THEN s.time_stamp
          ELSE NULL END AS min_temperature_ts
        FROM (sample s
          JOIN (SELECT
                  date(s.time_stamp, 'unixepoch', 'localtime') AS date_stamp,
                  s.station_id,
                  max(s.gust_wind_speed)                       AS max_gust_wind_speed,
                  min(s.temperature)                           AS min_temperature,
                  max(s.temperature)                           AS max_temperature,
                  avg(s.temperature)                           AS avg_temperature,
                  avg(s.average_wind_speed)                    AS avg_wind_speed,
                  sum(s.rainfall)                              AS rainfall,
                  sum(s.heat_dd)                               AS heat_degree_days,
                  sum(s.cool_dd)                               AS cool_degree_days
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
                    END * ((stn.sample_interval * 60) / 86400.0) AS cool_dd,
                    CASE WHEN x.temperature < p.heat_base
                      THEN p.heat_base - x.temperature
                    ELSE 0
                    END * ((stn.sample_interval * 60) / 86400.0) AS heat_dd
                  from parameters p, sample x
                  inner join station stn on stn.station_id = x.station_id
                  where x.time_stamp between p.start_date and p.end_date
                ) s, parameters p
                where s.time_stamp between p.start_date and p.end_date
                GROUP BY date(s.time_stamp, 'unixepoch', 'localtime'), s.station_id
               ) dr ON (date(s.time_stamp, 'unixepoch', 'localtime') = dr.date_stamp) and dr.station_id = s.station_id)
         WHERE s.gust_wind_speed = dr.max_gust_wind_speed OR
                 ((s.temperature = dr.max_temperature) OR (s.temperature = dr.min_temperature))
       ) data
  GROUP BY data.date_stamp, data.station_id, data.max_gust_wind_speed, data.max_temperature, data.min_temperature,
    data.avg_wind_speed, data.avg_temperature, data.rainfall, data.heat_degree_days, data.cool_degree_days
) as records on date(records.date) = dates.date and records.station_id = dates.station_id
LEFT OUTER JOIN (
    WITH wind_directions AS (
        SELECT
          date(time_stamp, 'unixepoch', 'localtime')            AS date,
          station_id,
          wind_direction,
          count(wind_direction) AS count
        FROM sample s, parameters p
        where s.time_stamp between p.start_date and p.end_date and s.wind_direction is not null
        GROUP BY date(time_stamp, 'unixepoch', 'localtime'), station_id, wind_direction
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