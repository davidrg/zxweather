WITH parameters AS (
    SELECT
      :fahrenheit AS fahrenheit,
      :mph        AS mph,
      :kmh        AS kmh,
      :inches     AS inches
), day_summary AS (
    SELECT
      s.station_id                                                            AS station_id,
      cast(strftime('%Y', s.time_stamp, 'unixepoch', 'localtime') AS INTEGER) AS year,
      cast(strftime('%m', s.time_stamp, 'unixepoch', 'localtime') AS INTEGER) AS month,
      cast(strftime('%d', s.time_stamp, 'unixepoch', 'localtime') AS INTEGER) AS day,
      max(s.temperature)                                                      AS max_temp,
      avg(s.temperature)                                                      AS avg_temp,
      min(s.temperature)                                                      AS min_temp,
      sum(s.rainfall)                                                         AS tot_rain,
      avg(s.relative_humidity)                                                AS avg_humidity,
      avg(s.average_wind_speed)                                               AS avg_wind
    FROM sample s
    GROUP BY
      s.station_id,
      cast(strftime('%Y', s.time_stamp, 'unixepoch', 'localtime') AS INTEGER),
      cast(strftime('%m', s.time_stamp, 'unixepoch', 'localtime') AS INTEGER),
      cast(strftime('%d', s.time_stamp, 'unixepoch', 'localtime') AS INTEGER)
), year_month_summary AS (
    SELECT
      ds.station_id        AS station_id,
      ds.year              AS year,
      ds.month             AS month,
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
      ds.month
), month_summary AS (
    SELECT
      yms.station_id        AS station_id,
      min(yms.year)         AS min_year,
      max(yms.year)         AS max_year,
      yms.month             AS month,
      CASE WHEN yms.month = 1
        THEN 'January'
      WHEN yms.month = 2
        THEN 'February'
      WHEN yms.month = 3
        THEN 'March'
      WHEN yms.month = 4
        THEN 'April'
      WHEN yms.month = 5
        THEN 'May'
      WHEN yms.month = 6
        THEN 'June'
      WHEN yms.month = 7
        THEN 'July'
      WHEN yms.month = 8
        THEN 'August'
      WHEN yms.month = 9
        THEN 'September'
      WHEN yms.month = 10
        THEN 'October'
      WHEN yms.month = 11
        THEN 'November'
      WHEN yms.month = 12
        THEN 'December'
      END                   AS month_name,
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
      yms.month
)
SELECT
  ms.min_year,
  ms.max_year,
  ms.month_name,
  cast(round(CASE WHEN p.fahrenheit
    THEN ms.avg_max_temp * 1.8 + 32
             ELSE ms.avg_max_temp END, 1) AS TEXT) AS avg_high,
  cast(round(CASE WHEN p.fahrenheit
    THEN ms.avg_temp * 1.8 + 32
             ELSE ms.avg_temp END, 1) AS TEXT)     AS daily_mean,
  cast(round(CASE WHEN p.fahrenheit
    THEN ms.avg_min_temp * 1.8 + 32
             ELSE ms.avg_min_temp END, 1) AS TEXT) AS avg_low,
  cast(round(CASE WHEN p.inches
    THEN ms.avg_tot_rain * 1.0 / 25.4
             ELSE ms.avg_tot_rain END, 1) AS TEXT) AS avg_precipitation,
  cast(round(ms.avg_rain_days, 1) AS TEXT)         AS avg_preciptiation_days,
  cast(round(ms.avg_humidity, 1) AS TEXT)          AS avg_humidity,
  cast(round(CASE WHEN p.kmh
    THEN ms.avg_wind * 3.6
             WHEN p.mph
               THEN ms.avg_wind * 2.23694
             ELSE ms.avg_wind END, 1) AS TEXT)     AS avg_wind,
  cast(round(ms.gale_days, 1) AS TEXT)             AS avg_gale_days
FROM month_summary ms, parameters p
  INNER JOIN station stn ON stn.station_id = ms.station_id
WHERE lower(stn.code) = lower(:stationCode)
ORDER BY ms.month
  ASC;
