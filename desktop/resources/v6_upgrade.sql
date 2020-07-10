alter table data_file add column is_complete integer default 0;

update db_metadata set v = 6 where k = 'v';
