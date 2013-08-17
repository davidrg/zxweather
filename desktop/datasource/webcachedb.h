#ifndef WEBCACHEDB_H
#define WEBCACHEDB_H

#include <QObject>
#include <QDateTime>

#include "datasource/sampleset.h"

typedef struct _data_file_t {
    QString filename;
    int size;
    QDateTime last_modified;
    SampleSet samples;
    bool isValid;
    bool expireExisting;
} data_file_t;

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
     * @return Any available data for the weather station between the
     *         requested times (including the requested times themselves).
     */
    SampleSet retrieveDataSet(QString stationUrl,
                              QDateTime startTime,
                              QDateTime endTime,
                              SampleColumns columns);

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
     */
    void cacheDataSet(SampleSet samples, int stationId, int dataFileId);

    /** Builds the select part of a query which includes the specified columns.
     * The timestamp column is always included.
     *
     * @param columns Columns to include in the select.
     * @return "select" followed by the specified columns.
     */
    QString buildSelectForColumns(SampleColumns columns);
};

#endif // WEBCACHEDB_H
