----------------------------------------------------------------------
-- DOMAINS -----------------------------------------------------------
----------------------------------------------------------------------
-- A percentage.
CREATE DOMAIN rh_percentage
  AS integer
  NOT NULL
   CONSTRAINT rh_percentage_check CHECK (((VALUE > 0) AND (VALUE < 100)));
ALTER DOMAIN rh_percentage
  OWNER TO postgres;
COMMENT ON DOMAIN rh_percentage
  IS 'Relative Humidity percentage (0-99%)';

-- Valid wind directions
CREATE DOMAIN wind_direction
  AS character varying(3)
  COLLATE pg_catalog."default"
  NOT NULL
   CONSTRAINT wind_direction_check CHECK (((VALUE)::text = ANY ((ARRAY['N'::character varying, 'NNE'::character varying, 'NE'::character varying, 'ENE'::character varying, 'E'::character varying, 'ESE'::character varying, 'SE'::character varying, 'SSE'::character varying, 'S'::character varying, 'SSW'::character varying, 'SW'::character varying, 'WSW'::character varying, 'W'::character varying, 'WNW'::character varying, 'NW'::character varying, 'NNW'::character varying, 'INV'::character varying])::text[])));
ALTER DOMAIN wind_direction
  OWNER TO postgres;
COMMENT ON DOMAIN wind_direction
  IS 'Valid wind directions.';

----------------------------------------------------------------------
-- TABLES ------------------------------------------------------------
----------------------------------------------------------------------
CREATE TABLE sample
(
  sample_id serial NOT NULL,
  sample_interval integer, -- Number of minutes since the previous sample
  record_number integer NOT NULL, -- The history slot in the weather stations circular buffer this record came from. Used for calculating the timestamp.
  download_timestamp timestamp with time zone, -- When this record was downloaded from the weather station
  last_in_batch boolean, -- If this was the last record in a batch of records downloaded from the weather station.
  invalid_data boolean NOT NULL, -- If the record from the weather stations "no sensor data received" flag is set.
  time_stamp timestamp with time zone, -- The calculated date and time when this record was likely recorded by the weather station. How far out it could possibly be depends on the size of the sample interval, etc.
  indoor_relative_humidity rh_percentage, -- Relative Humidity at the base station
  indoor_temperature real, -- Temperature at the base station
  relative_humidity rh_percentage, -- Relative Humidity
  temperature real, -- Temperature outside
  dew_point real, -- Calculated dew point
  wind_chill real, -- Calculated wind chill
  apparent_temperature real, -- Calculated apparent temperature.
  absolute_pressure real, -- Absolute pressure
  relative_pressure real, -- Calculated relative pressure.
  average_wind_speed real, -- Average Wind Speed.
  gust_wind_speed real, -- Gust wind speed.
  wind_direction wind_direction, -- Wind Direction.
  rainfall real, -- Calculated rainfall. Calculation is based on total_rainfall and rain_overflow columns compared to the previous sample.
  total_rain real, -- Total rain recorded by the sensor so far. Subtract the previous samples total rainfall from this one to calculate the amount of rain for this sample.
  rain_overflow boolean, -- If an overflow in the total_rain counter has occurred
  CONSTRAINT pk_sample PRIMARY KEY (sample_id )
);
ALTER TABLE sample
  OWNER TO postgres;
COMMENT ON TABLE sample
  IS 'Samples from the weather station.';
COMMENT ON COLUMN sample.sample_interval IS 'Number of minutes since the previous sample';
COMMENT ON COLUMN sample.record_number IS 'The history slot in the weather stations circular buffer this record came from. Used for calculating the timestamp.';
COMMENT ON COLUMN sample.download_timestamp IS 'When this record was downloaded from the weather station';
COMMENT ON COLUMN sample.last_in_batch IS 'If this was the last record in a batch of records downloaded from the weather station.';
COMMENT ON COLUMN sample.invalid_data IS 'If the record from the weather stations "no sensor data received" flag is set.';
COMMENT ON COLUMN sample.time_stamp IS 'The calculated date and time when this record was likely recorded by the weather station. How far out it could possibly be depends on the size of the sample interval, etc.';
COMMENT ON COLUMN sample.indoor_relative_humidity IS 'Relative Humidity at the base station. 0-99%';
COMMENT ON COLUMN sample.indoor_temperature IS 'Temperature at the base station in degrees Celsius';
COMMENT ON COLUMN sample.relative_humidity IS 'Relative Humidity. 0-99%';
COMMENT ON COLUMN sample.temperature IS 'Temperature outside in degrees Celsius';
COMMENT ON COLUMN sample.dew_point IS 'Calculated dew point in degrees Celsius. Use the dew_point function to calculate. Calculated automatically by a trigger on insert.';
COMMENT ON COLUMN sample.wind_chill IS 'Calculated wind chill in degrees Celsius. Use the wind_chill function to calculate. Calculated automatically by a trigger on insert.';
COMMENT ON COLUMN sample.apparent_temperature IS 'Calculated apparent temperature in degrees Celsius. Use the apparent_temperature to calculate. Calculated automatically by a trigger on insert.';
COMMENT ON COLUMN sample.absolute_pressure IS 'Absolute pressure in Hpa';
COMMENT ON COLUMN sample.relative_pressure IS 'Calculated relative pressure.';
COMMENT ON COLUMN sample.average_wind_speed IS 'Average Wind Speed in m/s.';
COMMENT ON COLUMN sample.gust_wind_speed IS 'Gust wind speed in m/s.';
COMMENT ON COLUMN sample.wind_direction IS 'Wind Direction.';
COMMENT ON COLUMN sample.rainfall IS 'Calculated rainfall in mm. Smallest recordable amount is 0.3mm. Calculation is based on total_rainfall and rain_overflow columns compared to the previous sample.';
COMMENT ON COLUMN sample.total_rain IS 'Total rain recorded by the sensor so far. Smallest possible increment is 0.3mm. Subtract the previous samples total rainfall from this one to calculate the amount of rain for this sample.';
COMMENT ON COLUMN sample.rain_overflow IS 'If an overflow in the total_rain counter has occurred';

----------------------------------------------------------------------
-- INDICIES ----------------------------------------------------------
----------------------------------------------------------------------

-- Cuts recalculating rainfall for 3000 records from 4200ms to 110ms.
CREATE INDEX idx_time_stamp
   ON sample (time_stamp ASC NULLS LAST);
COMMENT ON INDEX idx_time_stamp
  IS 'To make operations such as recalculating rainfall a little quicker.';

-- For the latest_record_number view.
--CREATE INDEX idx_latest_record
--   ON sample (time_stamp DESC NULLS FIRST, record_number DESC NULLS FIRST);
--COMMENT ON INDEX idx_latest_record
--  IS 'Index for the latest_record_number view';

----------------------------------------------------------------------
-- VIEWS -------------------------------------------------------------
----------------------------------------------------------------------

CREATE OR REPLACE VIEW latest_record_number AS
 SELECT record_number, time_stamp 
 from sample
 order by time_stamp desc, record_number desc
 limit 1;

ALTER TABLE latest_record_number
  OWNER TO postgres;
COMMENT ON VIEW latest_record_number
  IS 'Gets the number of the most recent record. This is used by cweather to figure out what it needs to load into the database and what is already there.';
----------------------------------------------------------------------
-- FUNCTIONS ---------------------------------------------------------
----------------------------------------------------------------------

-- Function to calculate dew point
CREATE OR REPLACE FUNCTION dew_point(temperature real, relative_humidity integer)
RETURNS real AS
$BODY$
DECLARE
  a real := 17.271;
  b real := 237.7; -- in degrees C
  gamma real;
  result real;
BEGIN
  gamma = ((a * temperature) / (b + temperature)) + ln(relative_humidity::real / 100.0);
  result = (b * gamma) / (a - gamma);
  return result;
END; $BODY$
LANGUAGE plpgsql IMMUTABLE;
COMMENT ON FUNCTION dew_point(real, integer) IS 'Calculates the approximate dew point given temperature relative humidity. The calculation is based on the August–Roche–Magnus approximation for the saturation vapor pressure of water in air as a function of temperature. It is valid for input temperatures 0 to 60 degC and dew points 0 to 50 deg C.';

-- Function to calculate wind chill
CREATE OR REPLACE FUNCTION wind_chill(temperature real, wind_speed real)
RETURNS real AS
$BODY$
DECLARE
  wind_kph real;
  result real;
BEGIN
  -- wind_speed is in m/s. Multiply by 3.6 to get km/h
  wind_kph := wind_speed * 3.6;

  -- This calculation is undefined for temperatures above 10degC or wind speeds below 4.8km/h
  if wind_kph <= 4.8 or temperature > 10.0 then
        return temperature;
  end if;

  result := 13.12 + (temperature * 0.6215) + (((0.3965 * temperature) - 11.37) * pow(wind_kph, 0.16));

  -- Wind chill is always lower than the temperature.
  if result > temperature then
        return temperature;
  end if;

  return result;
END; $BODY$
LANGUAGE plpgsql IMMUTABLE;
COMMENT ON FUNCTION wind_chill(real, real) IS 'Calculates the north american wind chill index. Formula from http://en.wikipedia.org/wiki/Wind_chill.';

-- Calculates the apparent temperature
CREATE OR REPLACE FUNCTION apparent_temperature(temperature real, wind_speed real, relative_humidity real)
RETURNS real AS
$BODY$
DECLARE
  wvp real;
  result real;
BEGIN

  -- Water Vapour Pressure in hPa
  wvp := (relative_humidity / 100.0) * 6.105 * pow(2.718281828, ((17.27 * temperature) / (237.7 + temperature)));

  result := temperature + (0.33 * wvp) - (0.7 * wind_speed) - 4.00;

  return result;
END; $BODY$
LANGUAGE plpgsql IMMUTABLE;
COMMENT ON FUNCTION apparent_temperature(real, real, real) IS 'Calculates the Apparent Temperature using the formula used for the Australian Bureau of Meteorology - http://www.bom.gov.au/info/thermal_stress/';

----------------------------------------------------------------------
-- TRIGGER FUNCTIONS -------------------------------------------------
----------------------------------------------------------------------

-- Computes any computed fields when a new sample is inserted (dew point, wind chill, aparent temperature, etc)
CREATE OR REPLACE FUNCTION compute_sample_values()
  RETURNS trigger AS
$BODY$
	BEGIN
                -- If its an insert, calculate any fields that need calculating. We will ignore updates here.
		IF(TG_OP = 'INSERT') THEN
                        -- Various calculated temperatures
                        NEW.dew_point = dew_point(NEW.temperature, NEW.relative_humidity);
                        NEW.wind_chill = wind_chill(NEW.temperature, NEW.average_wind_speed);
                        NEW.apparent_temperature = apparent_temperature(NEW.temperature, NEW.average_wind_speed, NEW.relative_humidity);

                        -- Calculate actual rainfall for this record from the total rainfall
                        -- accumulator of this record and the previous record.
                        -- 19660.8 is the maximum rainfall accumulator value (65536 * 0.3mm).
                        select into NEW.rainfall
                                    CASE WHEN NEW.total_rain - prev.total_rain >= 0 THEN
                                        NEW.total_rain - prev.total_rain
                                    ELSE
                                        NEW.total_rain + (19660.8 - prev.total_rain)
                                    END as rainfall
                        from sample prev
                        -- find the previous sample:
                        where time_stamp = (select max(time_stamp)
                                            from sample ins
                                            where ins.time_stamp < NEW.time_stamp);
		END IF;

		RETURN NEW;
	END;
$BODY$
  LANGUAGE plpgsql VOLATILE;
COMMENT ON FUNCTION compute_sample_values() IS 'Calculates values for all calculated fields (wind chill, dew point, rainfall, etc).';

----------------------------------------------------------------------
-- TRIGGERS ----------------------------------------------------------
----------------------------------------------------------------------

CREATE TRIGGER calculate_fields BEFORE INSERT
   ON sample FOR EACH ROW
   EXECUTE PROCEDURE public.compute_sample_values();
COMMENT ON TRIGGER calculate_fields ON sample IS 'Calculate any calculated fields.';



