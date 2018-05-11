with params as (
  select date_trunc('day', dd)::date as date,
         (:at_time)::time::varchar as time,
         extract(hour from (:offset)::timestamp)::int as time_offset,   -- TODO: Figure this out.
         (:latitude)::numeric as latitude,
         (:longitude)::numeric as longitude
  FROM generate_series(:start, :end, '1 day'::interval) dd
),
julian_date as (
  select date,
         latitude,
         longitude,
         ((extract(epoch from (params.date || ' ' || params.time || '+' || params.time_offset || ':00')::timestamptz) / 86400) +2440587.5) as julian_date
  from params
),
julian_cent as (
  select date,
         latitude,
         longitude,
         (julian_date - 2451545) / 36525 as julian_century
    from julian_date
  ),
mean_obliq_eliptic_deg as (
  select date,
         latitude,
         longitude,
          23 + (26 + ((21.448 - julian_century * (46.815 + julian_century * (0.00059 - julian_century * 0.001813)))) / 60) / 60 as mean_obliq_eliptic_deg
    from julian_cent
  ),
calcs as (  -- Misc calculations that mostly only depend on julian_cent
    select c.date,
           c.latitude,
           c.longitude,
           mean_obliq_eliptic_deg + 0.00256 * cos(radians(125.04 - 1934.136 * julian_century)) as obliq_corr_deg,
           mod((280.46646 + julian_century * (36000.76983 + julian_century * 0.0003032))::numeric, 360.0) as geom_mean_long_sun_deg,
           357.52911 + julian_century * (35999.05029 - 0.0001537 * julian_century) as geom_mean_anom_sun_deg,
           0.016708634 - julian_century * (0.000042037 + 0.0000001267 * julian_century) as eccent_earth_orbit
    from julian_cent c
    inner join mean_obliq_eliptic_deg m on m.date = c.date and m.latitude = c.latitude and m.longitude = c.longitude
  ),
sun_eq_of_ctr as (
    select c.date,
           c.latitude,
           c.longitude,
           sin(radians(geom_mean_anom_sun_deg))
                    * (1.914602 - julian_century * (0.004817 + 0.000014 * julian_century))
                    + sin(radians(2 * geom_mean_anom_sun_deg)) * (0.019993 - 0.000101 * julian_century)
                    + sin(radians(3 * geom_mean_anom_sun_deg)) * 0.000289 as sun_eq_of_ctr
    from julian_cent c
    inner join calcs calc on c.date = calc.date and c.latitude = calc.latitude and c.longitude = calc.longitude
  ),
sun_true_long_deg as (
    select calc.date,
           calc.latitude,
           calc.longitude,
           geom_mean_long_sun_deg + sun_eq_of_ctr as sun_true_long_deg
    from calcs calc
    inner join sun_eq_of_ctr ctr on ctr.date = calc.date and ctr.longitude = calc.longitude and ctr.latitude = calc.latitude
  ),
sun_app_long_deg as (
    select c.date,
           c.latitude,
           c.longitude,
           sun_true_long_deg - 0.00569 - 0.00478 * sin(radians(125.04 - 1934.136 * julian_century)) as sun_app_long_deg
    from julian_cent c
    inner join sun_true_long_deg st on c.date = st.date and c.latitude = st.latitude and c.longitude = st.longitude
),
  sun_decln_deg as (
    select calc.date,
           calc.latitude,
           calc.longitude,
           degrees(asin(sin(radians(obliq_corr_deg)) * sin(radians(sun_app_long_deg)))) as sun_decln_deg
    from calcs calc
    inner join sun_app_long_deg sa on sa.date = calc.date and sa.latitude = calc.latitude and sa.longitude = calc.longitude
  ),
hour_angles_deg as (
    select date,
           latitude,
           longitude,
           degrees(acos(cos(radians(90.833)) / (cos(radians(latitude)) * cos(radians(sun_decln_deg))) - tan(radians(latitude)) * tan(radians(sun_decln_deg)))) as ha_sunrise_deg,
           degrees(acos(cos(radians(96.0)) / (cos(radians(latitude)) * cos(radians(sun_decln_deg))) - tan(radians(latitude)) * tan(radians(sun_decln_deg)))) as ha_civil_dawn,
           degrees(acos(cos(radians(102.0)) / (cos(radians(latitude)) * cos(radians(sun_decln_deg))) - tan(radians(latitude)) * tan(radians(sun_decln_deg)))) as ha_nautical_dawn,
           degrees(acos(cos(radians(108.0)) / (cos(radians(latitude)) * cos(radians(sun_decln_deg))) - tan(radians(latitude)) * tan(radians(sun_decln_deg)))) as ha_astronomical_dawn
    from sun_decln_deg
  ),
var_y as (
    select date,
           latitude,
           longitude,
           tan(radians(obliq_corr_deg / 2)) * tan(radians(obliq_corr_deg / 2)) as var_y
    from calcs
  ),
eq_of_time_minutes as (
    select calc.date,
           calc.latitude,
           calc.longitude,
           4 * degrees(var_y * sin(2 * radians(geom_mean_long_sun_deg))
                       - 2 * eccent_earth_orbit * sin(radians(geom_mean_anom_sun_deg))
                       + 4 * eccent_earth_orbit * var_y * sin(radians(geom_mean_anom_sun_deg))
                       * cos(2 * radians(geom_mean_long_sun_deg)) - 0.5 * var_y * var_y
                       * sin(4 * radians(geom_mean_long_sun_deg)) - 1.25 * eccent_earth_orbit * eccent_earth_orbit
                       * sin(2 * radians(geom_mean_anom_sun_deg))) as eq_of_time_minutes
    from calcs calc
    inner join var_y vy on vy.date = calc.date and vy.latitude = calc.latitude and vy.longitude = calc.longitude
  ),
solar_noon_lst as (
    select param.date,
           param.latitude,
           param.longitude,
           (720 - 4 * param.longitude - eq_of_time_minutes + time_offset * 60) / 1440 as solar_noon_lst
    from params param
    inner join eq_of_time_minutes eq on eq.date = param.date and eq.latitude = param.latitude and eq.longitude = param.longitude
  ),
times as (
  select sn.date,
         sn.latitude,
         sn.longitude,
		 (TIMESTAMP 'epoch' + ((solar_noon_lst)* 86400) * INTERVAL '1 second')::time as solar_noon,
         (TIMESTAMP 'epoch' + (((solar_noon_lst * 1440 - ha_sunrise_deg * 4) / 1440)* 86400) * INTERVAL '1 second')::time as sunrise_time,
         (TIMESTAMP 'epoch' + (((solar_noon_lst * 1440 + ha_sunrise_deg * 4) / 1440)* 86400) * INTERVAL '1 second')::time as sunset_time,
         (TIMESTAMP 'epoch' + (((solar_noon_lst * 1440 - ha_civil_dawn * 4) / 1440)* 86400) * INTERVAL '1 second')::time as civil_dawn,
         (TIMESTAMP 'epoch' + (((solar_noon_lst * 1440 + ha_civil_dawn * 4) / 1440)* 86400) * INTERVAL '1 second')::time as civil_dusk,
         (TIMESTAMP 'epoch' + (((solar_noon_lst * 1440 - ha_nautical_dawn * 4) / 1440)* 86400) * INTERVAL '1 second')::time as nautical_dawn,
         (TIMESTAMP 'epoch' + (((solar_noon_lst * 1440 + ha_nautical_dawn * 4) / 1440)* 86400) * INTERVAL '1 second')::time as nautical_dusk,
         (TIMESTAMP 'epoch' + (((solar_noon_lst * 1440 - ha_astronomical_dawn * 4) / 1440)* 86400) * INTERVAL '1 second')::time as astronomical_dawn,
         (TIMESTAMP 'epoch' + (((solar_noon_lst * 1440 + ha_astronomical_dawn * 4) / 1440)* 86400) * INTERVAL '1 second')::time as astronomical_dusk
  from solar_noon_lst sn
  inner join hour_angles_deg ha on ha.date = sn.date and ha.longitude = sn.longitude and ha.latitude = sn.latitude
)
select
	case when times.date = NOW()::date then '<b>' || times.date::varchar || '</b>' else times.date::varchar end as date,
	case when times.date = NOW()::date then '<b>' || to_char(times.sunrise_time, 'HH12:MI:SS')::varchar || '</b>' else to_char(times.sunrise_time, 'HH12:MI:SS')::varchar end as rise_time,
	case when times.date = NOW()::date then '<b>' || to_char(times.sunset_time, 'HH12:MI:SS')::varchar || '</b>' else to_char(times.sunset_time, 'HH12:MI:SS')::varchar end as set_time,
	case when times.date = NOW()::date then '<b>' || to_char(times.solar_noon, 'HH12:MI:SS')::varchar || '</b>' else to_char(times.solar_noon, 'HH12:MI:SS')::varchar end as solar_noon,
	case when times.date = NOW()::date then '<b>' || to_char(times.civil_dawn, 'HH12:MI:SS')::varchar || '</b>' else to_char(times.civil_dawn, 'HH12:MI:SS')::varchar end as civil_dawn,
	case when times.date = NOW()::date then '<b>' || to_char(times.civil_dusk, 'HH12:MI:SS')::varchar || '</b>' else to_char(times.civil_dusk, 'HH12:MI:SS')::varchar end as civil_dusk,
	case when times.date = NOW()::date then '<b>' || to_char(times.nautical_dawn, 'HH12:MI:SS')::varchar || '</b>' else to_char(times.nautical_dawn, 'HH12:MI:SS')::varchar end as nautical_dawn,
	case when times.date = NOW()::date then '<b>' || to_char(times.nautical_dusk, 'HH12:MI:SS')::varchar || '</b>' else to_char(times.nautical_dusk, 'HH12:MI:SS')::varchar end as nautical_dusk,
	case when times.date = NOW()::date then '<b>' || to_char(times.astronomical_dawn, 'HH12:MI:SS')::varchar || '</b>' else to_char(times.astronomical_dawn, 'HH12:MI:SS')::varchar end as astronomical_dawn,
	case when times.date = NOW()::date then '<b>' || to_char(times.astronomical_dusk, 'HH12:MI:SS')::varchar || '</b>' else to_char(times.astronomical_dusk, 'HH12:MI:SS')::varchar end as astronomical_dusk
from times
order by times.date;