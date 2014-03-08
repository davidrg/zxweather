#ifndef DBUTIL_H
#define DBUTIL_H

#include <QString>
#include <QSqlDatabase>
#include <QDateTime>
#include <QList>
#include <QMetaType>

/** Misc utility functions for dealing with a weather database.
 *
 */
namespace DbUtil {

/** Result from a database compatibility check.
 */
typedef enum {
    DC_Compatible, /*!< Database is fully compatible */
    DC_Incompatible, /*!< Database is incompatible */
    DC_BadSchemaVersion, /*!< Failed to determine database schema version. Incompatible. */
    DC_Unknown /*!< Could not determine compatibility. Might work. Might not. */
} DatabaseCompatibility;

/** Checks to see if the specified database is compatible with this version
 * of the zxweather desktop client.
 *
 * @param db Database to check compatibility of.
 * @return Result of the database compatibility check.
 */
DatabaseCompatibility checkDatabaseCompatibility(QSqlDatabase db);

/** Gets the minimum version of the desktop client required to connect to
 * the specified database. The minimum version is returned as a human-readable
 * string (eg, 1.5.2)
 *
 * @param db Database to get the minimum app version for.
 * @return The minimum version of the app required.
 */
QString getMinimumAppVersion(QSqlDatabase db);

/** Gets the database schema version. Used to check if the database is new
 * enough for this version of the desktop client.
 *
 * @param db Database to get the schema version of.
 * @return Database schema version.
 */
int getDatabaseVersion(QSqlDatabase db);

/** Information about a weather station.
 */
struct StationInfo {
    QString code; /*!< Station code */
    QString title; /*!< Station name/title */
    QString description; /*!< Station description (long, possibly with html) */
    int sampleInterval; /*!< Sample interval */
    bool liveDataAvailable; /*!< If live data is available or not */
    int sortOrder; /*!< Sort order (0 should be at the top) */
    QString stationTypeCode; /*!< The station type code */
    QString stationTypeName; /*!< Human-readable name for the station type */
};

/** Gets a list of all weather stations available in the database.
 *
 * @param db Database to get a list of weather stations for.
 * @return A list of weather stations.
 */
QList<StationInfo> getStationList(QSqlDatabase db);
}

Q_DECLARE_METATYPE(DbUtil::StationInfo)
Q_DECLARE_METATYPE(QList<DbUtil::StationInfo>)

#endif // DBUTIL_H
