
#include "dbutil.h"

#include <QSqlQuery>
#include <QSqlRecord>
#include <QSqlError>
#include <QVariant>
#include <QtDebug>

using namespace DbUtil;


int DbUtil::getDatabaseVersion(QSqlDatabase db) {
    QSqlQuery query("select * "
                    "from information_schema.tables "
                    "where table_schema = 'public' "
                    "  and table_name = 'db_info'", db);
    if (!query.isActive()) {
        return -1;
    } else if (query.size() == 1){
        // It is at least a v2 (zxweather 0.2) schema.
        query.exec("select v::integer "
                   "from DB_INFO "
                   "where k = 'DB_VERSION'");
        if (!query.isActive() || query.size() != 1) {
            return -1;
        } else {
            query.first();
            return query.value(0).toInt();
        }
    }

    return 1;
}

DatabaseCompatibility DbUtil::checkDatabaseCompatibility(QSqlDatabase db) {
    int version = getDatabaseVersion(db);
    qDebug() << "Schema version:" << version;
    if (version == -1) {
        return DbUtil::DC_BadSchemaVersion;
    } else if (version == 1) {
        qDebug() << "V1 database";
        /*
         * While the live data bits of DatabaseDataSource all work fine with
         * a v1 schema all the code dealing with samples assumes a v2 schema
         * (it relies on the station_id column) so until that is fixed the v1
         * schema is incompatible.
         */
        return DbUtil::DC_Incompatible;
    } else if (version > 1) {
        qDebug() << "V2+ database.";

        // Check that this version of the desktop client hasn't been
        // blacklisted by the database.
        QSqlQuery query("select version_check('desktop',1,0,0)", db);
        if (!query.isActive())
            return DbUtil::DC_Unknown;
        else {
            query.first();
            if (!query.value(0).toBool()) {
                return DbUtil::DC_Incompatible;
            }
        }
    }
    return DbUtil::DC_Compatible;
}


QString DbUtil::getMinimumAppVersion(QSqlDatabase db)
{
    QSqlQuery query("select minimum_version_string('desktop')", db);

    QString result;

    if (query.isActive()) {
        query.first();
        result = query.value(0).toString();
    }

    return result;
}

QList<StationInfo> DbUtil::getStationList(QSqlDatabase db) {
    QList<StationInfo> result;

    int schemaVersion = getDatabaseVersion(db);

    if (schemaVersion <= 1) {
        /* We're on a zxweather v0.1 database. It only supports a single
         * FineOffset WH1080-compatbile weather station. The station code
         * is not stored in the database (and not at all required when working
         * with the database) so we'll just make up a fake station info record.
         */

        StationInfo si;
        si.code = "unkn";
        si.sampleInterval = 300; // 5 minutes
        si.liveDataAvailable = true;
        si.sortOrder = 0;
        si.stationTypeCode = "FOWH1080";
        si.stationTypeName = "FineOffset WH1080-compatible";
        result << si;
        return result;
    }

    QString qry =
       "select s.code,"
       "        s.title,"
       "        s.description,"
       "        s.sample_interval,"
       "        s.live_data_available,"
       "        s.sort_order,"
       "        st.code as station_type_code,"
       "        st.title as station_type_name "
       "from station s "
       "inner join station_type st on st.station_type_id = s.station_type_id "
       "order by sort_order asc";
    QSqlQuery query(qry, db);

    while (query.next()) {
        StationInfo si;
        QSqlRecord record = query.record();
        si.code = record.value("code").toString();
        si.title = record.value("title").toString();
        si.description = record.value("description").toString();
        si.sampleInterval = record.value("sample_interval").toInt();
        si.liveDataAvailable = record.value("live_data_available").toBool();
        si.sortOrder = record.value("sort_order").toInt();
        si.stationTypeCode = record.value("station_type_code").toString();
        si.stationTypeName = record.value("station_type_name").toString();
        result << si;
    }

    return result;
}
