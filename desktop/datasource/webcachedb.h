#ifndef WEBCACHEDB_H
#define WEBCACHEDB_H

#include <QObject>
#include <QDateTime>
#include <QVector>

#include "datasource/sampleset.h"
#include "datasource/imageset.h"
#include "datasource/aggregate.h"
#include "datasource/station_info.h"
#include "datasource/abstractprogresslistener.h"

typedef struct _data_file_t {
    QString filename;
    int size;
    QDateTime last_modified;
    SampleSet samples;
    bool isValid;
    bool expireExisting;
    bool hasSolarData;
    bool isComplete;
    QDateTime start_contiguous_to;
    QDateTime end_contiguous_from;

    // This should be the very start of the day or month
    // eg: 30-JUL-2021 00:00:00
    QDateTime start_time;

    // This should be the very end of the day or month
    // eg: 30-JUL-2021 23:59:59
    QDateTime end_time;
} data_file_t;

typedef struct _sample_gap_t {
    QDateTime start_time;
    QDateTime end_time;
    int missing_samples;
    QString label;

    bool operator==(struct _sample_gap_t const & rhs) const {
        // As far as equality goes we really only care about the timespan.
        // Missing sample count depends on timespan and label doesn't affect caching.
        return this->start_time == rhs.start_time && this->end_time == this->end_time;
    }
} sample_gap_t;

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

    static QSqlQuery query();

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
                              uint32_t groupMinutes,
                              AbstractProgressListener *progressListener = 0);

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

    /** Updates the stored image information with the supplied values
     *
     * @param imageInfo ID of the image to update
     */
    void updateImageInfo(QString stationUrl, ImageInfo imageInfo);

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

    /** Gets the most recent image for each image source attached to the
     * specified station.
     *
     * @param stationUrl Station to get most recent images for
     * @return List of most recent images
     */
    QVector<ImageInfo> getMostRecentImages(QString stationUrl);

    /** Clears all samples from the database.
     */
    void clearSamples();

    /** Clears all image metadata from the database.
     */
    void clearImages();

    /** Returns true if the specified station is known to the cache database
     *
     * @param url Station URL
     * @return true if it exists in the cache database
     */
    bool stationKnown(QString url);

    /** Returns true if the specified station is known to the cache database
     *  and marked as archived.
     *
     * @param url Station URL
     * @return If the station exists and is marked as archived.
     */
    bool stationIsArchived(QString url);

    /** Updates extra data associated with a station. Aside from the solar and hardware type
     * parameters this is mostly for use by reports.
     *
     * @param url Station to update
     * @param title Station title
     * @param description Description
     * @param type_code Hardware type code
     * @param interval Sample interval in MINUTES
     * @param latitude Latitude. Use FLT_MAX if unknown.
     * @param longitude Longitude. use FLT_MAX if unknown.
     * @param altitude Altitude. use 0 if unknown.
     * @param solar If solar sensors are available
     * @param davis_broadcast_id Wireless broadcast ID for davis stations. Use 0 for cabled or non-davis stations.
     * @param extraColumnNames Names for all enabled extra columns. Extra columns not included will be considered disabled.
     */
    void updateStation(QString url, QString title, QString description, QString type_code,
                       int interval, float latitude, float longitude, float altitude,
                       bool solar, int davis_broadcast_id,
                       QMap<ExtraColumn, QString> extraColumnNames, bool archived, QDateTime archivedTime, QString archivedMessage, unsigned int apiLevel);


    /** Updates the list of known permanent data gaps for the specified
     *  station.
     *
     * @param url Station URL
     * @param gaps Permanent data gap information.
     */
    void updateStationGaps(QString url, QList<sample_gap_t> gaps);

    /** Gets the list of all known permanent data gaps for the specified station
     *
     * @param url Station to get data gaps for
     * @return List of data gaps
     */
    QList<sample_gap_t> getStationGaps(QString url);

    /** Checks the database to see if the specified timespan falls within
     *  a known permanent gap in the stations full dataset.
     *
     * @param url Station URL
     * @param gapStart Start of the timespan to check
     * @param gapEnd End of the timespan to check
     * @return If the timespan is or falls within a known permanent gap.
     */
    bool sampleGapIsKnown(QString url, QDateTime gapStart, QDateTime gapEnd);

    /** Gets the names for all enabled extra senosrs
     *
     * @param url Station to get extra sensor names for
     * @return Map of sensor to name
     */
    QMap<ExtraColumn, QString> getExtraColumnNames(QString url);

    /** Returns true if the station has solar sensors available
     *
     * @param url Station to check
     * @return True if solar sensors are available
     */
    bool solarAvailable(QString url);

    /** Gets the hardware type associated with the station
     *
     * @param url Station to check
     * @return Station hardware type code
     */
    QString hw_type(QString url);

    /** Gets basic station info
     *
     * @param url The station to get info for
     * @return Station info
     */
    station_info_t getStationInfo(QString url);

    /** Gets the sample interval in minutes for the specified station
     *
     * @param url Station URL
     * @return Sample interval in seconds
     */
    int getSampleInterval(QString url);

    /** Gets the range of samples available for the specified station
     *
     * @param url Station to get the range for
     * @return Start and end times for the station
     */
    sample_range_t getSampleRange(QString url);

    /** Stores the list of dates that have images available. This is
     * primarily for the benefit of reports.
     *
     * @param stationCode Station the date list is being updated for.
     * @param dates Map of dates that have images available and the number of images
     *              available for those dates.
     */
    void updateImageDateList(QString stationCode, QMap<QString, QMap<QDate, int> > dates);

    bool imageSourceDateIsCached(QString stationUrl, QString sourceCode, QDate date);


    /** Returns true if all samples from the specified start time to the specified
     *  end time (inclusive) are present in the cache database.
     *
     * @param stationUrl Station to check cache for
     * @param startTime start time of timespan
     * @param endTime end time of timespan
     * @return If all samples are available.
     */
    bool timespanIsCached(QString stationUrl, QDateTime startTime, QDateTime endTime);

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
     * @param isComplete If the data file covers its full timespan completely (no gaps)
     * @param startContiguousTo There are no gaps between the start of the file and
     *              this time (inclusive). If null/invalid then there is a gap at the
     *              start of the file.
     * @param endContiguousFrom There are no gaps between this time and the end of the
     *              file (inclusive). If null/invlaid then there is a gap at the end of
     *              the file.
     */
    void updateDataFile(int fileId, QDateTime lastModified, int size, bool isComplete,
                        QDateTime startContiguousTo, QDateTime endContiguousFrom);

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

    bool runDbScript(QString filename);

    /** Gets the weather stations sample interval in seconds.
     * @param stationId the station to get the sample interval for
     * @return Sample interval in seconds
     */
    int getSampleInterval(int stationId);

    /** Optimises the database.
     *
     */
    void optimise();

    /** Runs the specified database upgrade script.
     *
     * @param version The version this script is for. Script will only run if
     *                current DB version is below this number.
     * @param script Script file to run to perform the upgrade
     * @param filename Name of the database file being upgraded. This is
     *                 used in diagnostic messages only.
     */
    bool runUpgradeScript(int version, QString script, QString filename);

    /** If the cache DB is ready.
     */
    bool ready;
};

#endif // WEBCACHEDB_H
