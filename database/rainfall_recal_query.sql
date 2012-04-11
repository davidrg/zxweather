update sample set rainfall = rfcalc.rainfall
from (select s.sample_id, s.time_stamp, s.total_rain, rain.rainfall
from sample s,
(select rf_data.sample_id,
       CASE WHEN rf_data.total_rain - rf_data.prev_total_rain >= 0 THEN
                rf_data.total_rain - rf_data.prev_total_rain
            ELSE
                rf_data.total_rain + (19660.8 - rf_data.prev_total_rain)
            END as rainfall
from (
select this.sample_id, 
       this.time_stamp,
       this.total_rain, 
       prev.sample_id as prev_sample_id,
       prev.time_stamp as prev_time_stamp,
       prev.total_rain as prev_total_rain
from sample this,
     sample prev
where
prev.time_stamp = (select max(time_stamp)
                   from sample ins
                   where ins.time_stamp < this.time_stamp)
) as rf_data) as rain
where rain.sample_id = s.sample_id) as rfcalc
where sample.sample_id = rfcalc.sample_id