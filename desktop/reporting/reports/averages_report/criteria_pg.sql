select
  case when :fahrenheit = true then '°F' else '°C' end as temperature,
  case when :mph = true then 'mph' when :kmh = true then 'km/h' else 'm/s' end as wind_speed,
  case when :inches = true then 'in' else 'mm' end as rainfall
