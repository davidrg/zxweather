WITH parameters AS (
   SELECT
     :start::date                       AS start,
     :end::date                         AS end_date,
     lower(:stationCode::varchar(5))    AS station_code,
     :fahrenheit::boolean	              AS fahrenheit, -- instead of celsius
   	 :inches::boolean		                AS inches, -- instead of mm
     32::INTEGER                        AS max_high_temp,
     0::INTEGER                         AS max_low_temp,
     0::INTEGER                         AS min_high_temp,
     -18::INTEGER                       AS min_low_temp,
     0.2                                AS rain_02,
     2.0                                AS rain_2,
     20.0                               AS rain_20
), ranges AS (
  select
    case when p.fahrenheit then (p.max_high_temp * 1.8 + 32)::integer
         else p.max_high_temp end as max_high_temp,
    case when p.fahrenheit then (p.max_low_temp * 1.8 + 32)::integer
         else p.max_low_temp end as max_low_temp,
    case when p.fahrenheit then (p.min_high_temp * 1.8 + 32)::integer
         else p.min_high_temp end as min_high_temp,
    case when p.fahrenheit then (p.min_low_temp * 1.8 + 32)::integer
         else p.min_low_temp end as min_low_temp,
    p.station_code
  from parameters p
)
select
  sum(y.max_high_temp)::varchar as max_high_temp_days,
  sum(y.max_low_temp)::varchar as max_low_temp_days,
  sum(y.min_high_temp)::varchar as min_high_temp_days,
  sum(y.min_low_temp)::varchar as min_low_temp_days,
  sum(y.rain_ex_02)::varchar as rain_ex_02_days,
  sum(y.rain_ex_2)::varchar as rain_ex_2_days,
  sum(y.rain_ex_20)::varchar as rain_ex_20_days,
  lpad(round(r.max_high_temp::numeric, 1)::varchar, 5, ' ') as max_high_temp,
  lpad(round(r.max_low_temp::numeric, 1)::varchar, 5, ' ') as max_low_temp,
  lpad(round(r.min_high_temp::numeric, 1)::varchar, 5, ' ') as min_high_temp,
  lpad(round(r.min_low_temp::numeric, 1)::varchar, 5, ' ') as min_low_temp,
  case when p.inches then to_char(round(p.rain_02  * 1.0/25.4, 2), 'FM9.99') || ' in' else to_char(p.rain_02, 'FM9.9') || ' mm' end as rain_02,
  case when p.inches then to_char(round(p.rain_2   * 1.0/25.4, 1), 'FM9.9') || ' in' else round(p.rain_2, 0)   || ' mm' end as rain_2,
  case when p.inches then round(p.rain_20  * 1.0/25.4, 0) || ' in' else round(p.rain_20, 0)  || ' mm' end as rain_20
from (
  SELECT
    CASE WHEN x.max_temp >= r.max_high_temp THEN 1 ELSE 0 END AS max_high_temp,
    CASE WHEN x.max_temp <= r.max_low_temp  THEN 1 ELSE 0 END AS max_low_temp,
    CASE WHEN x.min_temp <= r.min_high_temp THEN 1 ELSE 0 END AS min_high_temp,
    CASE WHEN x.min_temp <= r.min_low_temp  THEN 1 ELSE 0 END AS min_low_temp,
    CASE WHEN x.tot_rain > 0.2              THEN 1 ELSE 0 END AS rain_ex_02,
    CASE WHEN x.tot_rain > 2.0              THEN 1 ELSE 0 END AS rain_ex_2,
    CASE WHEN x.tot_rain > 20.0             THEN 1 ELSE 0 END AS rain_ex_20
  FROM (
         SELECT
           s.time_stamp :: DATE as date,
           max(s.temperature) AS max_temp,
           min(s.temperature) AS min_temp,
           sum(s.rainfall)    AS tot_rain
         FROM sample s
           INNER JOIN station stn ON stn.station_id = s.station_id
           INNER JOIN parameters p ON p.station_code = stn.code
           INNER JOIN ranges r ON r.station_code = p.station_code
         WHERE s.time_stamp :: DATE BETWEEN p.start AND p.end_date
         GROUP BY s.time_stamp :: DATE
       ) AS x, ranges r, parameters p
) as y, ranges r, parameters p
group by r.max_high_temp, r.max_low_temp, r.min_high_temp, r.min_low_temp, p.rain_2, p.rain_02, p.rain_20, p.inches