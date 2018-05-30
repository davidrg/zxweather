select
  case when :fahrenheit = 1 then '°F' else '°C' end as temperature,
  case when :mph = 1 then 'mph' when :kmh = 1 then 'km/h' else 'm/s' end as wind_speed,
  case when :inches = 1 then 'in' else 'mm' end as rainfall
