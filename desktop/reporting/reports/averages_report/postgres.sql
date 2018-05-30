WITH parameters AS (
    SELECT
      :fahrenheit::boolean AS fahrenheit,
      :mph::boolean        AS mph,
      :kmh::boolean        AS kmh,
      :inches::boolean     AS inches
), day_summary AS (
    SELECT
      s.station_id                     AS station_id,
      extract(YEAR FROM s.time_stamp)  AS year,
      extract(MONTH FROM s.time_stamp) AS month,
      to_char(s.time_stamp, 'Month')   AS month_name,
      extract(DAY FROM s.time_stamp)   AS day,
      max(s.temperature)               AS max_temp,
      avg(s.temperature)               AS avg_temp,
      min(s.temperature)               AS min_temp,
      sum(s.rainfall)                  AS tot_rain,
      avg(s.relative_humidity)         AS avg_humidity,
      avg(s.average_wind_speed)        AS avg_wind
    FROM sample s
    GROUP BY
      s.station_id, s.time_stamp
), year_month_summary AS (
    SELECT
      ds.station_id        AS station_id,
      ds.year              AS year,
      ds.month             AS month,
      ds.month_name        AS month_name,
      max(ds.max_temp)     AS max_temp,
      avg(ds.avg_temp)     AS avg_temp,
      min(ds.min_temp)     AS min_temp,
      sum(ds.tot_rain)     AS tot_rain,
      count(CASE WHEN ds.tot_rain >= 1.0
        THEN 1
            ELSE NULL END) AS rain_days,
      avg(ds.avg_humidity) AS avg_humidity,
      avg(ds.avg_wind)     AS avg_wind,
      count(CASE WHEN ds.avg_wind >= 17.5
        THEN 1
            ELSE NULL END) AS gale_days
    FROM day_summary ds
    GROUP BY
      ds.station_id,
      ds.year,
      ds.month,
      ds.month_name
), month_summary AS (
    SELECT
      yms.station_id        AS station_id,
      min(yms.year)         AS min_year,
      max(yms.year)         AS max_year,
      yms.month             AS month,
      yms.month_name        AS month_name,
      avg(yms.max_temp)     AS avg_max_temp,
      avg(yms.avg_temp)     AS avg_temp,
      avg(yms.min_temp)     AS avg_min_temp,
      avg(yms.tot_rain)     AS avg_tot_rain,
      avg(yms.rain_days)    AS avg_rain_days,
      -- Average number of days where the average rain for that day was >= 1.0mm
      avg(yms.avg_humidity) AS avg_humidity,
      avg(yms.avg_wind)     AS avg_wind,
      sum(yms.gale_days)    AS gale_days -- Average number of days where the average wind for that day was >= 63km/h
    FROM year_month_summary yms
    GROUP BY
      yms.station_id,
      yms.month,
      yms.month_name
)
SELECT
  ms.min_year,
  ms.max_year,
  ms.month_name,
  round(CASE WHEN p.fahrenheit = TRUE
    THEN ms.avg_max_temp * 1.8 + 32
        ELSE ms.avg_max_temp END :: NUMERIC, 1) :: VARCHAR AS avg_high,
  round(CASE WHEN p.fahrenheit = TRUE
    THEN ms.avg_temp * 1.8 + 32
        ELSE ms.avg_temp END :: NUMERIC, 1) :: VARCHAR     AS daily_mean,
  round(CASE WHEN p.fahrenheit = TRUE
    THEN ms.avg_min_temp * 1.8 + 32
        ELSE ms.avg_min_temp END :: NUMERIC, 1) :: VARCHAR AS avg_low,
  round(CASE WHEN p.inches = TRUE
    THEN ms.avg_tot_rain * 1.0 / 25.4
        ELSE ms.avg_tot_rain END :: NUMERIC, 1) :: VARCHAR AS avg_precipitation,
  round(ms.avg_rain_days, 1) :: VARCHAR                    AS avg_preciptiation_days,
  round(ms.avg_humidity, 1) :: VARCHAR                     AS avg_humidity,
  round(CASE WHEN p.kmh = TRUE
    THEN ms.avg_wind * 3.6
        WHEN p.mph = TRUE
          THEN ms.avg_wind * 2.23694
        ELSE ms.avg_wind END :: NUMERIC, 1) :: VARCHAR     AS avg_wind,
  round(ms.gale_days, 1) :: VARCHAR                        AS avg_gale_days
FROM parameters p, month_summary ms
  INNER JOIN station stn ON stn.station_id = ms.station_id
WHERE stn.code = :stationCode
ORDER BY ms.month
ASC;
