-- ZXweather v0.1.2 database schema upgrade for 0.1.0 and 0.1.1
begin;

ALTER DOMAIN rh_percentage
  DROP NOT NULL;

-- Move the percentage validation onto the table so that it is processed
-- *after* triggers fire.
alter domain rh_percentage drop constraint if exists rh_percentage_check;

ALTER TABLE sample
ADD CONSTRAINT chk_outdoor_relative_humidity
CHECK (relative_humidity > 0 and relative_humidity < 100);
COMMENT ON CONSTRAINT chk_outdoor_relative_humidity ON sample
IS 'Ensure the outdoor relative humidity is in the range 0-100';

ALTER TABLE sample
ADD CONSTRAINT chk_indoor_relative_humidity
CHECK (indoor_relative_humidity > 0 and indoor_relative_humidity < 100);
COMMENT ON CONSTRAINT chk_indoor_relative_humidity ON sample
IS 'Ensure the indoor relative humidity is in the range 0-100';

-- This function will now reset all outdoor data fields to null if the
-- invalid data flag is set.
CREATE OR REPLACE FUNCTION compute_sample_values()
  RETURNS trigger AS
$BODY$
	BEGIN
                -- If its an insert, calculate any fields that need calculating. We will ignore updates here.
		IF(TG_OP = 'INSERT') THEN

		    IF(NEW.invalid_data) THEN
		        -- The outdoor sample data in this record is garbage. Discard
		        -- it or we'll get constraint violations
                NEW.relative_humidity = null;
                NEW.temperature = null;
                NEW.dew_point = null;
                NEW.wind_chill = null;
                NEW.apparent_temperature = null;
                NEW.average_wind_speed = null;
                NEW.gust_wind_speed = null;
                NEW.rainfall = null;
                -- Wind direction should be fine (it'll be 'INV').
                -- The rest of the fields are captured by the base station
                -- and so should be fine.
		    ELSE
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

            NOTIFY new_sample;
		END IF;

		RETURN NEW;
	END;
$BODY$
  LANGUAGE plpgsql VOLATILE
  COST 100;
COMMENT ON FUNCTION compute_sample_values() IS 'Calculates values for all calculated fields (wind chill, dew point, rainfall, etc).';

commit;
