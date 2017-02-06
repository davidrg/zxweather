#ifndef WEBCACHEDB_H
#define WEBCACHEDB_H

#include <QObject>
#include <QDateTime>
#include <QVector>

#include "datasource/sampleset.h"
#include "datasource/imageset.h"
#include "datasource/aggregate.h"

typedef struct _data_file_t {
    QString filename;
    int size;
    QDateTime last_modified;
    SampleSet samples;
    bool isValid;
    bool expireExisting;
    bool hasSolarData;
} data_file_t;

typedef struct _image_set_t {
    QString filename;
    int size;
    QDateTime last_modified;
    QString station_url;
    ImageSource source;
    QVector<ImageInfo> images;
    bool is_valid;
} image_set_t;

typedef struct _cache_stats_t {
    int count;
    QDateTime start;
    QDateTime end;
    bool isValid;
} cache_stats_t;

/* How caching works:
 *  - Only download data file if either:
 *     + Its not present in the DB at all
 *     + Its timestamp (use HTTP HEAD command) does not match that stored
 *       in the database
 *  - To decide what needs caching:
 *     1. Grab the min and max timestamp for that file in the DB as well
 *        as the record count.
 *     2. Ensure the record count between those timestamps matches what
 *        is in the data file. If it matches then that range does not need
 *        to be cached.
 *     3. If the range count does not match then drop the entire data file
 *        from the DB and re-cache it
 *     4. If the range count *DOES* match then cache all samples falling
 *        outside the range only.
 * This should mean we don't need to check for individual samples. It
 * should also make it easier to decide what needs downloading and what
 * doesn't.
 */

class QSqlQuery;

/** Provides access to the cache database used by the web data source.
 * WebCacheDB is a singleton. Call getInstance() to grab the instance.
 *
 */
class WebCacheDB : public QObject
{
    Q_OBJECT
public:
    /** Returns the one allowed instance of the WebCacheDB class.
     *
     * @return WebClassDB instance.
     */
    static WebCacheDB& getInstance() {
        static WebCacheDB instance;
        return instance;
    }

    /** Retrieves data between the specified times for the specified weather
     * station. If no data is available in the database within the time range
     * for the weather station then no data will be returned.
     *
     * @param stationUrl The URL identifying the weather station to fetch data
     *                   for (eg, http://weather.zx.net.nz/b/rua2/)
     * @param startTime Start time.
     * @param endTime End time.
     * @param columns The columns to return.
     * @param aggregateFunction The aggregate function to use (if any)
     * @param aggregateGroupType Timespan to group by
     * @param groupMinutes Custom grouping value in minutes (for AGT_Custom)
     * @return Any available data for the weather station between the
     *         requested times (including the requested times themselves).
     */
    SampleSet retrieveDataSet(QString stationUrl,
                              QDateTime startTime,
                              QDateTime endTime,
                              SampleColumns columns,
                              AggregateFunction aggregateFunction,
                              AggregateGroupType aggregateGroupType,
                              uint32_t groupMinutes);

    /** Stores the specified data file in the cache database. If the file does
     * not exist it will be created, otherwise it will be updated. All samples
     * will be inserted. If the expireExisting member is set on dataFile then
     * any existing cache data for the file will be purged first.
     *
     * @param dataFile The data file to store or update in the database.
     * @param stationUrl The URL identifying the weather station
     *                   (eg, http://weather.zx.net.nz/b/rua2/)
     */
    void cacheDataFile(data_file_t dataFile, QString stationUrl);

    /** Gets some basic cache related information about the specified data
     * file. This is the last modified timestamp and the data files original
     * size when last downloaded.
     *
     * @param dataFileUrl The URL of the data file to get information on.
     * @return Basic information about the specified data file if it is
     *         present in the database. If it could not be found in the
     *         database then the isValid member will be set to false.
     */
    data_file_t getDataFileCacheInformation(QString dataFileUrl);

    /** Gets some basic statics about the cache contents for the specified
     * file. This information is primarily for deciding if the cache is
     * up-to-date or not. The information includes the earliest available
     * record, the latest record and the total record count for the file.
     *
     * @param dataFileUrl The data file URL to get information for.
     * @return Cache statistics for the file.
     */
    cache_stats_t getCacheStats(QString dataFileUrl);

    /** Stores the specified image set in the cache database. If the image set
     * does not exist it will be created, otherwise it will be updated. All
     * new images will be inserted.
     *
     * @param imageSet The image set to cache
     * @param stationUrl The weather station the image set came from
     */
    void cacheImageSet(image_set_t imageSet);


    /** Stores a single image against a temporary image set. The image may be
     * re-parented into the appropriate image set later on.
     *
     * @param stationUrl URL for the weather station the image came from
     * @param image Image information to store
     */
    void storeImageInfo(QString stationUrl, ImageInfo image);

    /** Get information about the specified image.
     *
     * @param stationUrl URL for the weather station the image is assocaited with
     * @param id Image ID to get metadata for
     * @return Metadata for the specified image
     */
    ImageInfo getImageInfo(QString stationUrl, int id);

    /** Gets the specified image source.
     *
     * @param stationUrl Station the image source is associated with
     * @param sourceCode Image sources identifying code
     * @return Image source information. If code field is not populated, the
     *  image source could not be found in the cache database.
     */
    ImageSource getImageSource(QString stationUrl, QString sourceCode);

    /** Gets imagse by date, station and image source code
     *
     * @param date Image set date
     * @param stationUrl Station the image set is for
     * @param imageSourceCode Image source the image set is for
     * @return Requested images
     */
    QVector<ImageInfo> getImagesForDate(QDate date, QString stationUrl,
                                        QString imageSourceCode);

    /** Gets cache statistics for an image set by its URL
     *
     * @param imageSetUrl URL of the image set to get cache info for
     * @return Cache info for the specified image set
     */
    image_set_t getImageSetCacheInformation(QString imageSetUrl);

signals:
    /** Emitted when an error occurs which would prevent the cache database
     * from operating.
     *
     * @param message
     */
    void criticalError(QString message);

private:
    /*
     * Enforce singleton-ness
     */
    WebCacheDB();
    ~WebCacheDB();
    WebCacheDB(WebCacheDB const&); /* Not implemented. Don't use it. */
    void operator=(WebCacheDB const&); /* Not implemented. Don't use it. */

    /** Opens the cache database. If the database does not exist then it will
     * be created. If the database is for an older version it will be upgraded.
     */
    void openDatabase();

    /** Creates the table structure in a new database.
     */
    void createTableStructure();

    /** Gets the station ID in the cache database. If the station does not
     * already exist in the database it is created and its ID returned.
     *
     * @param stationUrl The URL of the station
     *                   (eg, http://weather.zx.net.nz/b/rua2/)
     * @return Station ID for use when querying the sampe and cache_entries
     *         tables.
     */
    int getStationId(QString stationUrl);

    /** Gets the ID for the specified image source in the specified station
     *
     * @param stationId Weather station ID
     * @param code Image source code
     * @return Image source ID or -1 if it was not found
     */
    int getImageSourceId(int stationId, QString code);

    /** Creates the specified image source if it doesn't exist and returns its
     * Id
     *
     * @param stationId Station the image source is assocaited with
     * @param source Image source to create
     * @return ID of the created (or existing) image source
     */
    int createImageSource(int stationId, ImageSource source);

    /** Updates the cached name and description for the specified image source
     *
     * @param imageSourceId Image source to update
     * @param name New name
     * @param description New description
     */
    void updateImageSourceInfo(int imageSourceId, QString name, QString description);

    /** Returns true if the specified image set exists
     *
     * @param url Image set URL
     * @return true if the set exists
     */
    int getImageSetId(QString url);

    /** Updates stored information about the supplied image set
     *
     * @param imageSet The image set to update
     */
    void updateImageSetInfo(image_set_t imageSet);

    /** Stores the specified image set in the database
     *
     * @param imageSet The image set to store
     */
    int storeImageSetInfo(image_set_t imageSet, int imageSourceId);

    /** Returns true if the specified image exists within the specified station
     *
     * @param stationUrl Station to check for the image in
     * @param id Image ID
     * @return True if the image exists
     */
    bool imageExists(int stationId, int id);

    /** Stores image metadata
     *
     * @param image Image metadata
     * @param imageSetId ID of the image set to store the image metadata against
     */
    void storeImage(ImageInfo image, int imageSetId, int stationId, int imageSourceId);

    /** Gets the cache file ID in the cache database. If the file does not
     * already exist then -1 is returned.
     *
     * @param dataFileUrl File URL
     * @return File ID or -1 if file does not exist.
     */
    int getDataFileId(QString dataFileUrl);

    /** Creates a new data file record in the database. This is used to track
     * where cache entries came from.
     *
     * @param dataFile Details about the data file to create.
     * @param stationId ID of the station that supplied the data file.
     * @return ID of the new data file.
     */
    int createDataFile(data_file_t dataFile, int stationId);

    /** Updates details about a data file in the cache database.
     *
     * @param fileId ID of the file record to update
     * @param lastModified The new last modified date of the file.
     * @param size The new size of the file.
     */
    void updateDataFile(int fileId, QDateTime lastModified, int size);

    /** Drops all cache data associated with the specified file from the
     * database.
     *
     * @param fileId ID of the file to truncate.
     */
    void truncateFile(int fileId);

    /** Stores the supplied samples against the specified data file in the
     * cache database.
     *
     * @param samples The samples to store
     * @param stationId The Station the samples belong to
     * @param dataFileId The data file the samples belong to
     * @param hasSolarData Is UV Index and Solar Radiation available
     */
    void cacheDataSet(SampleSet samples, int stationId, int dataFileId,
                      bool hasSolarData);

    /** Builds a list of columns for the specified column set using the
     * supplied format string. In the format string, %1 is the column name.
     * @param columns Columns to include in the column list
     * @param format Format string for building a single column spec.
     * @return List of columns.
     */
    QString buildColumnList(SampleColumns columns, QString format);

    /** Builds the select part of a query which includes the specified columns.
     * The timestamp column is always included.
     *
     * @param columns Columns to include in the select.
     * @return "select" followed by the specified columns.
     */
    QString buildSelectForColumns(SampleColumns columns);

    /** Gets the row count for a non-aggregated data set.
     *
     * @param stationId Station to get row count for
     * @param startTime start time for data range
     * @param endTime end time for data range
     * @return Row count. -1 if there was an error.
     */
    int getNonAggregatedRowCount(int stationId, QDateTime startTime, QDateTime endTime);

    /** Builds the base aggregated select query used for row counts and data retreival.
     *
     * @param columns Columns to fetch. Pass SC_None for row counts.
     * @param function Aggregate function to use
     * @param groupType Time span grouping.
     * @param groupMinutes Number of minutes to group by for AGT_Custom
     * @return An aggregated SQL query.
     */
    QString buildAggregatedSelect(SampleColumns columns, AggregateFunction function,
                                  AggregateGroupType groupType);

    /** Gets the row count for an aggregated data set.
     *
     * @param stationId Station to get the row count for
     * @param startTime Start time for data range
     * @param endTime End time for data range
     * @param aggregateFunction Aggregate function to use (sum, min, max, etc)
     * @param groupType Grouping to use (day, week, month, etc)
     * @param groupMinutes Number of minutes to group by for custom grouping.
     *
     * @return Row count. -1 if there was an error.
     */
    int getAggregatedRowCount(int stationId, QDateTime startTime, QDateTime endTime,
                              AggregateFunction aggregateFunction,
                              AggregateGroupType groupType, uint32_t groupMinutes);

    /** Builds a basic query to fetch data without any aggregation.
     * @param columns Columns to fetch.
     * @return Data query
     */
    QSqlQuery buildBasicSelectQuery(SampleColumns columns);

    /** Builds a query that fetches aggregated data.
     *
     * @param columns Columns to fetch
     * @param stationId Station to fetch data for.
     * @param aggregateFunction Aggregation function to use
     * @param groupType Time period to group by
     * @param groupMinutes Number of minutes to group by if groupType is AGT_Custom
     * @return Data query
     */
    QSqlQuery buildAggregatedSelectQuery(SampleColumns columns, int stationId,
                                         AggregateFunction aggregateFunction,
                                         AggregateGroupType groupType, uint32_t groupMinutes);
};

#endif // WEBCACHEDB_H
