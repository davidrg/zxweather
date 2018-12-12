function generate_datasets(criteria) {
	return [
		generate_solar_dataset(criteria),
		generate_lat_long_dataset(criteria)
	]
}