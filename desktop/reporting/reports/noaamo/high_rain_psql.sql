WITH parameters AS (
   SELECT
     :start::date                       AS start,
     :end::date                         AS end_date,
   	 :inches::boolean		                AS inches, -- instead of mm
     lower(:stationCode::varchar(5))   AS station_code
 ), monthly_rain as (
  select
    s.time_stamp::date   as date,
    sum(s.rainfall)     as total_rain,
    p.inches             as inches
  from sample s
  inner join station stn on stn.station_id = s.station_id
  inner join parameters p on p.station_code = stn.code
    where s.time_stamp::date between p.start and p.end_date
  group by s.time_stamp::date, p.inches
)
 select
   mr.date::date as date,
	 case when mr.inches then round((mr.total_rain * 1.0/25.4)::NUMERIC, 2)
		   else round(mr.total_rain::NUMERIC, 1)
	 end::varchar as rain
from monthly_rain mr
inner join
  (
  select max(total_rain) as max
  from monthly_rain
) as max_rain on max_rain.max = mr.total_rain
;