select asg.gap_start_time + asg.sample_interval as first_missing_sample,
       asg.gap_end_time - asg.sample_interval as last_missing_sample,
       asg.gap_length as gap_length,
       asg.missing_samples as missing_samples,
       case when asg.is_known_gap then 'Yes' else 'No' end as is_known,
       asg.label as label
from get_all_sample_gaps(:stationCode) as asg
order by asg.gap_start_time desc;