select
  yearly_data.year as year,
  substr('        ' || coalesce(cast(round(yearly_data.january, 1) as text), '---'), -6) as january,
  substr('        ' || coalesce(cast(round(yearly_data.february, 1) as text), '---'), -6) as february,
  substr('        ' || coalesce(cast(round(yearly_data.march, 1) as text), '---'), -6) as march,
  substr('        ' || coalesce(cast(round(yearly_data.april, 1) as text), '---'), -6) as april,
  substr('        ' || coalesce(cast(round(yearly_data.may, 1) as text), '---'), -6) as may,
  substr('        ' || coalesce(cast(round(yearly_data.june, 1) as text), '---'), -6) as june,
  substr('        ' || coalesce(cast(round(yearly_data.july, 1) as text), '---'), -6) as july,
  substr('        ' || coalesce(cast(round(yearly_data.august, 1) as text), '---'), -6) as august,
  substr('        ' || coalesce(cast(round(yearly_data.september, 1) as text), '---'), -6) as september,
  substr('        ' || coalesce(cast(round(yearly_data.october, 1) as text), '---'), -6) as october,
  substr('        ' || coalesce(cast(round(yearly_data.november, 1) as text), '---'), -6) as november,
  substr('        ' || coalesce(cast(round(yearly_data.december, 1) as text), '---'), -6) as december,
  substr('        ' || coalesce(cast(round(yearly_data.total, 1) as text), '---'), -7) as total
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
      select cast(strftime('%Y', s.time_stamp, 'unixepoch') as integer) as year,
        cast(strftime('%m', s.time_stamp, 'unixepoch') as integer) as month,
        sum(s.rainfall) as rainfall
      from sample s
      inner join station stn on stn.station_id = s.station_id
      where lower(stn.code) = lower(:stationCode)
        and cast(strftime('%Y', s.time_stamp, 'unixepoch') as integer) between cast(strftime('%Y', :start_t, 'unixepoch') as integer) and cast(strftime('%Y', :end_t, 'unixepoch') as integer)
      group by year, month
    ) as rain_data
  ) as pivoted
  group by pivoted.year
) as yearly_data
order by year asc;