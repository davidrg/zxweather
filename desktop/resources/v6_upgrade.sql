alter table data_file add column is_complete integer default 0;

alter table image_dates add column server_image_count integer default null;

update db_metadata set v = 6 where k = 'v';
