WITH parameters AS (
  SELECT
	:celsius::boolean		AS celsius,
	:fahrenheit::boolean    AS fahrenheit,
	:kmh::boolean			AS kmh,
	:mph::boolean			AS mph,
	:inches::boolean		AS inches,
	:atDate::date      		AS date,
    :title::varchar		    AS title,
    :city::varchar 			AS city,
    :state::varchar   		AS state,
    :altitude::FLOAT        AS altitude,
    :latitude::FLOAT        AS latitude,
    :longitude::FLOAT       AS longitude,
	:altFeet::BOOLEAN       AS altitude_feet,
	:coolBase::FLOAT		AS cool_base,
	:heatBase::FLOAT		AS heat_base,
	32::INTEGER             AS max_high_temp,
    0::INTEGER              AS max_low_temp,
    0::INTEGER              AS min_high_temp,
    -18::INTEGER            AS min_low_temp,
    0.2                     AS rain_02,
    2.0                     AS rain_2,
    20.0                    AS rain_20
),
latitude_dms as (
  select floor(abs(latitude)) as degrees,
         floor(60 * (abs(latitude) - floor(abs(latitude)))) as minutes,
         3600 * (abs(latitude) - floor(abs(latitude))) - 60 * floor(60 * (abs(latitude) - floor(abs(latitude)))) as seconds,
         case when latitude < 0 then 'S' else 'N' end as direction
  from parameters
),
longitude_dms as (
  select floor(abs(longitude)) as degrees,
         floor(60 * (abs(longitude) - floor(abs(longitude)))) as minutes,
         3600 * (abs(longitude) - floor(abs(longitude))) - 60 * floor(60 * (abs(longitude) - floor(abs(longitude)))) as seconds,
         case when longitude < 0 then 'W' else 'E' end as direction
  from parameters
), ranges AS (
  select
    case when p.fahrenheit then (p.max_high_temp * 1.8 + 32)::integer
         else p.max_high_temp end as max_high_temp,
    case when p.fahrenheit then (p.max_low_temp * 1.8 + 32)::integer
         else p.max_low_temp end as max_low_temp,
    case when p.fahrenheit then (p.min_high_temp * 1.8 + 32)::integer
         else p.min_high_temp end as min_high_temp,
    case when p.fahrenheit then (p.min_low_temp * 1.8 + 32)::integer
         else p.min_low_temp end as min_low_temp
  from parameters p
)
select
  to_char(p.date, 'MON')                  AS month,
  extract(year from p.date)               AS year,
  p.title                                 AS title,
  p.city                                  AS city,
  p.state                                 AS state,
  lpad(round((
    CASE WHEN p.altitude_feet THEN p.altitude * 3.28084
         ELSE p.altitude
    END)::NUMERIC, 0)::varchar, 5, ' ')   AS altitude,
  CASE WHEN p.altitude_feet THEN 'ft'
       ELSE 'm '
    END                                   AS altitude_units,
  lpad(lat.degrees || '° ' || lat.minutes || ''' ' || round(lat.seconds::numeric, 0) || '" ' || lat.direction, 14, ' ')      AS latitude,
  lpad(long.degrees || '° ' || long.minutes || ''' ' || round(long.seconds::numeric, 0) || '" ' || long.direction, 14, ' ')  AS longitude,
  CASE WHEN p.celsius THEN '°C'
       ELSE '°F' END                      AS temperature_units,
  CASE WHEN p.inches THEN 'in'
       ELSE 'mm' END                      AS rain_units,
  CASE WHEN p.kmh THEN 'km/h'
       WHEN p.mph THEN 'mph'
       ELSE 'm/s' END                     AS wind_units,
  lpad(round((
	CASE WHEN p.celsius THEN p.cool_base
	     ELSE p.cool_base * 1.8 + 32 
	END)::numeric, 1)::varchar, 5, ' ')   AS cool_base,
  lpad(round((
    CASE WHEN p.celsius THEN p.heat_base
         ELSE p.heat_base * 1.8 + 32 
	END)::numeric, 1)::varchar, 5, ' ')   AS heat_base,
  lpad(round(r.max_high_temp::numeric, 1)::varchar, 5, ' ') as max_high_temp,
  lpad(round(r.max_low_temp::numeric, 1)::varchar, 5, ' ') as max_low_temp,
  lpad(round(r.min_high_temp::numeric, 1)::varchar, 5, ' ') as min_high_temp,
  lpad(round(r.min_low_temp::numeric, 1)::varchar, 5, ' ') as min_low_temp,
  case when p.inches then to_char(round(p.rain_02  * 1.0/25.4, 2), 'FM9.99') || ' in' else to_char(p.rain_02, 'FM9.9') || ' mm' end as rain_02_value,
  case when p.inches then to_char(round(p.rain_2   * 1.0/25.4, 1), 'FM9.9') || ' in' else round(p.rain_2, 0)   || ' mm' end as rain_2_value,
  case when p.inches then round(p.rain_20  * 1.0/25.4, 0) || ' in' else round(p.rain_20, 0)  || ' mm' end as rain_20_value
from parameters p, latitude_dms as lat, longitude_dms as long, ranges as r;