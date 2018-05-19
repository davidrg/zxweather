WITH parameters AS (
  SELECT
    :celsius                AS celsius,
    :fahrenheit             AS fahrenheit,
    :kmh                    AS kmh,
    :mph                    AS mph,
    :inches                 AS inches,
    :atDate                 AS date,
    :title                  AS title,
    :city                   AS city,
    :state                  AS state,
    :altitude * 1.0         AS altitude,
    :latitude * 1.0         AS latitude,
    :longitude * 1.0        AS longitude,
    :altFeet                AS altitude_feet,
    :coolBase * 1.0         AS cool_base,
    :heatBase * 1.0         AS heat_base,
    32                      AS max_high_temp,
    0                       AS max_low_temp,
    0                       AS min_high_temp,
    -18                     AS min_low_temp,
    0.2                     AS rain_02,
    2.0                     AS rain_2,
    20.0                    AS rain_20
), floory as (
  select
    -- floor(abs(latitude)) in postgres
    case when abs(latitude) >= 0 then cast(abs(latitude) as int)
         when cast(abs(latitude) as int) = abs(latitude) then cast(abs(latitude) as int)
         else cast(abs(latitude) - 1.0 as int) end as floor_abs_latitude,
    -- floor(abs(longitude)) in postgres
    case when abs(longitude) >= 0 then cast(abs(longitude) as int)
     when cast(abs(longitude) as int) = abs(longitude) then cast(abs(longitude) as int)
     else cast(abs(longitude) - 1.0 as int) end as floor_abs_long
  from parameters
), floory_lat as (
  SELECT
    -- floor(60 * (abs(latitude) - floor_abs_latitude)) in postgres
    case when (60 * (abs(latitude) - floor_abs_latitude)) >= 0 then cast((60 * (abs(latitude) - floor_abs_latitude)) as int)
     when cast((60 * (abs(latitude) - floor_abs_latitude)) as int) = (60 * (abs(latitude) - floor_abs_latitude)) then cast((60 * (abs(latitude) - floor_abs_latitude)) as int)
     else cast((60 * (abs(latitude) - floor_abs_latitude)) - 1.0 as int) end as minutes
  from parameters, floory
), floory_long as (
  SELECT
    -- floor(60 * (abs(longitude) - floor(abs(longitude)))) in postgres
    case when (60 * (abs(longitude) - floor_abs_long)) >= 0 then cast((60 * (abs(longitude) - floor_abs_long)) as int)
     when cast((60 * (abs(longitude) - floor_abs_long)) as int) = (60 * (abs(longitude) - floor_abs_long)) then cast((60 * (abs(longitude) - floor_abs_long)) as int)
     else cast((60 * (abs(longitude) - floor_abs_long)) - 1.0 as int) end as minutes
  from parameters, floory
),
latitude_dms as (
  select cast(round(floor_abs_latitude, 0) as integer) as degrees,
         cast(round(minutes, 0) as integer) as minutes,
         cast(round(3600 * (abs(latitude) - floor_abs_latitude) - 60 * minutes, 0) as integer) as seconds,
         case when latitude < 0 then 'S' else 'N' end as direction
  from parameters, floory, floory_lat
),
longitude_dms as (
  select cast(round(floory.floor_abs_long, 0) as integer) as degrees,
         cast(round(floory_long.minutes, 0) as integer) as minutes,
         cast(round(3600 * (abs(parameters.longitude) - floory.floor_abs_long) - 60 * floory_long.minutes, 0) as integer) as seconds,
         case when longitude < 0 then 'W' else 'E' end as direction
  from parameters, floory, floory_long
)
select
  case when strftime('%m', datetime(p.date)) = '01' then 'JAN'
       when strftime('%m', datetime(p.date)) = '02' then 'FEB'
       when strftime('%m', datetime(p.date)) = '03' then 'MAR'
       when strftime('%m', datetime(p.date)) = '04' then 'APR'
       when strftime('%m', datetime(p.date)) = '05' then 'MAY'
       when strftime('%m', datetime(p.date)) = '06' then 'JUN'
       when strftime('%m', datetime(p.date)) = '07' then 'JUL'
       when strftime('%m', datetime(p.date)) = '08' then 'AUG'
       when strftime('%m', datetime(p.date)) = '09' then 'SEP'
       when strftime('%m', datetime(p.date)) = '10' then 'OCT'
       when strftime('%m', datetime(p.date)) = '11' then 'NOV'
       when strftime('%m', datetime(p.date)) = '12' then 'DEC'
  end                                     AS month,
  substr(strftime('%Y ', datetime(p.date)), -3)
                                          AS year,
  p.title                                 AS title,
  p.city                                  AS city,
  p.state                                 AS state,
  substr('     ' || cast(round((
    CASE WHEN p.altitude_feet THEN p.altitude * 3.28084
         ELSE p.altitude
    END), 0) as integer), -5)    AS altitude,
  CASE WHEN p.altitude_feet THEN 'ft'
       ELSE 'm '
    END                                   AS altitude_units,
  substr('                ' || lat.degrees || '° ' || lat.minutes || ''' ' || lat.seconds || '" ' || lat.direction, -14)
                                          AS latitude,
  substr('                ' || long.degrees || '° ' || long.minutes || ''' ' || long.seconds || '" ' || long.direction, -14)
                                          AS longitude,
  CASE WHEN p.celsius THEN '°C'
       ELSE '°F' END                      AS temperature_units,
  CASE WHEN p.inches THEN 'in'
       ELSE 'mm' END                      AS rain_units,
  CASE WHEN p.kmh THEN 'km/h'
       WHEN p.mph THEN 'mph'
       ELSE 'm/s' END                     AS wind_units,
  substr('     ' || round((
    CASE WHEN p.celsius THEN p.cool_base
         ELSE p.cool_base * 1.8 + 32
    END), 1), -4)             AS cool_base,
  substr('     ' || round((
    CASE WHEN p.celsius THEN p.heat_base
         ELSE p.heat_base * 1.8 + 32
    END), 1), -4)             AS heat_base,
  -- >=90 <=32 <=32 <=0
  -- >=32 <=0  <=0  <=-18
  '>=' || substr('     ' || case when p.fahrenheit then cast(round((p.max_high_temp * 1.8 + 32),0) as integer)
   else p.max_high_temp end, -2) as max_high_temp,
  '<=' || substr('     ' || case when p.fahrenheit then cast((p.max_low_temp * 1.8 + 32) as integer)
   else p.max_low_temp end, -2) as max_low_temp,
  '<=' || substr('     ' || case when p.fahrenheit then cast((p.min_high_temp * 1.8 + 32) as integer)
   else p.min_high_temp end, -2) as min_high_temp,
  '<=' || case when p.fahrenheit then cast((p.min_low_temp * 1.8 + 32) as integer)
   else p.min_low_temp end as min_low_temp,
   
  substr('     ' || case when p.inches then substr(round(p.rain_02  * 1.0/25.4, 2), -3) 
						 else               substr(p.rain_02, -2) end, -3)
          as rain_02,
  substr('     ' || case when p.inches then substr(round(p.rain_2   * 1.0/25.4, 1), -2) 
                         else               cast(round(p.rain_2, 0) as integer) end, -2) as rain_2,

  substr('     ' || case when p.inches then cast(round(p.rain_20  * 1.0/25.4, 0) as integer)
                         else               cast(round(p.rain_20, 0) as integer) end, -2) as rain_20
from parameters p, latitude_dms as lat, longitude_dms as long;