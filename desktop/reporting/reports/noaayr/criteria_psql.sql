WITH parameters AS (
  SELECT
	:celsius::boolean		    AS celsius,
	:fahrenheit::boolean    AS fahrenheit,
	:kmh::boolean			      AS kmh,
	:mph::boolean			      AS mph,
	:inches::boolean		    AS inches,
	:atDate::date      		  AS date,
  :title::varchar		      AS title,
  :city::varchar 			    AS city,
  :state::varchar   		  AS state,
  :altitude::FLOAT        AS altitude,
  :latitude::FLOAT        AS latitude,
  :longitude::FLOAT       AS longitude,
	:altFeet::BOOLEAN       AS altitude_feet,
	:coolBase::FLOAT		    AS cool_base,
	:heatBase::FLOAT		    AS heat_base,
	32::integer             AS max_high_temp,
  0::integer              AS max_low_temp,
  0::integer              AS min_high_temp,
  -18::integer            AS min_low_temp,
  0.2::numeric            AS rain_02,
  2.0::numeric            AS rain_2,
  20.0::numeric           AS rain_20
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
	END)::numeric, 1)::varchar, 4, ' ')   AS cool_base,
  lpad(round((
    CASE WHEN p.celsius THEN p.heat_base
         ELSE p.heat_base * 1.8 + 32
	END)::numeric, 1)::varchar, 4, ' ')   AS heat_base,
  -- >=90 <=32 <=32 <=0
  -- >=32 <=0  <=0  <=-18
  '>=' || rpad(case when p.fahrenheit then (p.max_high_temp * 1.8 + 32)::integer
   else p.max_high_temp end::varchar, 2, ' ') as max_high_temp,
  '<=' || rpad(case when p.fahrenheit then (p.max_low_temp * 1.8 + 32)::integer
   else p.max_low_temp end::varchar, 2, ' ') as max_low_temp,
  '<=' || rpad(case when p.fahrenheit then (p.min_high_temp * 1.8 + 32)::integer
   else p.min_high_temp end::varchar, 2, ' ') as min_high_temp,
  '<=' || case when p.fahrenheit then (p.min_low_temp * 1.8 + 32)::integer
   else p.min_low_temp end::varchar as min_low_temp,
  lpad(case when p.inches then to_char(round(p.rain_02  * 1.0/25.4, 2), 'FM9.99') else to_char(p.rain_02, 'FM9.9') end::varchar, 3, ' ') as rain_02,
  lpad(case when p.inches then to_char(round(p.rain_2   * 1.0/25.4, 1), 'FM9.9') else round(p.rain_2, 0)::varchar end::varchar, 2, ' ') as rain_2,
  lpad(case when p.inches then round(p.rain_20  * 1.0/25.4, 0)  else round(p.rain_20, 0) end::varchar, 2, ' ') as rain_20
from parameters p, latitude_dms as lat, longitude_dms as long;