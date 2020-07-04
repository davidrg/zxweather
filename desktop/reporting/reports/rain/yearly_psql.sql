select
  yearly_data.year as year,
  lpad(coalesce(cast(round(yearly_data.january::numeric, 1) as text), '---'), 6, ' ') as january,
  lpad(coalesce(cast(round(yearly_data.february::numeric, 1) as text), '---'), 6, ' ') as february,
  lpad(coalesce(cast(round(yearly_data.march::numeric, 1) as text), '---'), 6, ' ') as march,
  lpad(coalesce(cast(round(yearly_data.april::numeric, 1) as text), '---'), 6, ' ') as april,
  lpad(coalesce(cast(round(yearly_data.may::numeric, 1) as text), '---'), 6, ' ') as may,
  lpad(coalesce(cast(round(yearly_data.june::numeric, 1) as text), '---'), 6, ' ') as june,
  lpad(coalesce(cast(round(yearly_data.july::numeric, 1) as text), '---'), 6, ' ') as july,
  lpad(coalesce(cast(round(yearly_data.august::numeric, 1) as text), '---'), 6, ' ') as august,
  lpad(coalesce(cast(round(yearly_data.september::numeric, 1) as text), '---'), 6, ' ') as september,
  lpad(coalesce(cast(round(yearly_data.october::numeric, 1) as text), '---'), 6, ' ') as october,
  lpad(coalesce(cast(round(yearly_data.november::numeric, 1) as text), '---'), 6, ' ') as november,
  lpad(coalesce(cast(round(yearly_data.december::numeric, 1) as text), '---'), 6, ' ') as december,
  lpad(coalesce(cast(round(yearly_data.total::numeric, 1) as text), '---'), 7, ' ') as total
from (
  select pivoted.year,
    max(pivoted.january) as january,
    max(pivoted.february) as february,
    max(pivoted.march) as march,
    max(pivoted.april) as april,
    max(pivoted.may) as may,
    max(pivoted.june) as june,
    max(pivoted.july) as july,
    max(pivoted.august) as august,
    max(pivoted.september) as september,
    max(pivoted.october) as october,
    max(pivoted.november) as november,
    max(pivoted.december) as december,
    sum(pivoted.rainfall) as total
  from (
    select rain_data.year as year,
      case when rain_data.month = 1 then rain_data.rainfall else null end as january,
      case when rain_data.month = 2 then rain_data.rainfall else null end as february,
      case when rain_data.month = 3 then rain_data.rainfall else null end as march,
      case when rain_data.month = 4 then rain_data.rainfall else null end as april,
      case when rain_data.month = 5 then rain_data.rainfall else null end as may,
      case when rain_data.month = 6 then rain_data.rainfall else null end as june,
      case when rain_data.month = 7 then rain_data.rainfall else null end as july,
      case when rain_data.month = 8 then rain_data.rainfall else null end as august,
      case when rain_data.month = 9 then rain_data.rainfall else null end as september,
      case when rain_data.month = 10 then rain_data.rainfall else null end as october,
      case when rain_data.month = 11 then rain_data.rainfall else null end as november,
      case when rain_data.month = 12 then rain_data.rainfall else null end as december,
      rain_data.rainfall as rainfall
    from (
      select extract(year from s.time_stamp) as year,
        extract(month from s.time_stamp) as month,
        sum(s.rainfall) as rainfall
      from sample s
      inner join station stn on stn.station_id = s.station_id
      where lower(stn.code) = lower(:stationCode)
        and s.time_stamp between to_timestamp(:start_t) and to_timestamp(:end_t)
      group by year, month
    ) as rain_data
  ) as pivoted
  group by pivoted.year
) as yearly_data
order by year asc;