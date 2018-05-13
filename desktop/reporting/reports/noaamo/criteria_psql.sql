WITH parameters AS (
  SELECT
	:celsius::boolean		AS celsius,
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
	:heatBase::FLOAT		AS heat_base
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
       ELSE 'm'
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
	END)::numeric, 1)::varchar, 5, ' ')   AS heat_base
from parameters p, latitude_dms as lat, longitude_dms as long;