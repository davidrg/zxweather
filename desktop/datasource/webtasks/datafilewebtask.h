#ifndef FETCHSAMPLESSUBTASKS_H
#define FETCHSAMPLESSUBTASKS_H

#include "abstractwebtask.h"
#include "request_data.h"

#include <QObject>

class DataFileWebTask : public AbstractWebTask
{
    Q_OBJECT
public:
    /** Constucts a new AbstractWebTask
     *
     * @param baseUrl The base URL for the web interface
     * @param stationCode Station Code for the weather station being used
     * @param ds Parent data source that this task is doing work for
     * @param sampleInterval Stations sample interval in seconds
     */
    explicit DataFileWebTask(QString baseUrl,
                             QString stationCode,
                             request_data_t requestData,
                             QString name,
                             QString url,
                             bool forceDownload,
                             int sampleInterval,
                             WebDataSource* ds);

    /** Starts processing this task.
     */
    void beginTask();

    /** This is the maximum number of subtasks this task could perform. It will
     * be called shortly before beginTask(). It is used to calculate how much
     * space on a progress bar this task should be assigned.
     *
     * @return The maximum number of subtasks this task could perform.
     */
    int subtasks() const { return 3; }

    /** Name of this task. Used as the first line in a one line progress dialog
     * or the second line (shared with subtasks) in a two-line progress dialog.
     *
     * @return Name of this task or this tasks first subtask.
     */
    QString taskName() const {
        return QString(tr("Checking cache status of %1")).arg(_name);
    }

    /** Compares the result from an HTTP HEAD request to the cache database
     * and determinse if the local cache for the resource is out of date
     *
     * @param reply HTTP HEAD response
     * @return True if the cache is out-of-date and the URL needs to be downloaded
     */
    static bool UrlNeedsDownloading(QNetworkReply *reply);

public slots:
    /** Called when a network reply for a request this task submitted has
     * been received.
     *
     * It is the tasks responsibility to delete this reply once it has finished
     * with it.
     *
     * @param reply The network reply
     */
    void networkReplyReceived(QNetworkReply *reply);

private:

    enum DataFileColumn {
        DFC_TimeStamp = 1,
        DFC_Temperature = 2,
        DFC_DewPoint = 3,
        DFC_ApparentTemperature = 4,
        DFC_WindChill = 5,
        DFC_RelHumidity = 6,
        DFC_AbsolutePressure = 7,
        DFC_MSLPressure = 8,
        DFC_IndoorTemperature = 9,
        DFC_IndoorRelHumidity = 10,
        DFC_Rainfall = 11,
        DFC_AvgWindSpeed = 12,
        DFC_GustWindSpeed = 13,
        DFC_WindDirection = 14,
        DFC_UVIndex = 15,
        DFC_SolarRadiation = 16,
        DFC_Reception = 17,
        DFC_HighTemp = 18,
        DFC_LowTemp = 19,
        DFC_HighRainRate = 20,
        DFC_GustDirection = 21,
        DFC_Evapotranspiration = 22,
        DFC_HighSolarRadiation = 23,
        DFC_HighUVIndex = 24,
        DFC_ForecastRuleID = 25,
        DFC_SoilMoisture1 = 26,
        DFC_SoilMoisture2 = 27,
        DFC_SoilMoisture3 = 28,
        DFC_SoilMoisture4 = 29,
        DFC_SoilTemperature1 = 30,
        DFC_SoilTemperature2 = 31,
        DFC_SoilTemperature3 = 32,
        DFC_SoilTemperature4 = 33,
        DFC_LeafWetness1 = 34,
        DFC_LeafWetness2 = 35,
        DFC_LeafTemperature1 = 36,
        DFC_LeafTemperature2 = 37,
        DFC_ExtraHumidity1 = 38,
        DFC_ExtraHumidity2 = 39,
        DFC_ExtraTemperature1 = 40,
        DFC_ExtraTemperature2 = 41,
        DFC_ExtraTemperature3 = 42
    };

    // Parameters
    request_data_t _requestData;
    QString _url;
    QString _name;

    bool _downloadingDataset;
    bool _forceDownload;
    int _sampleInterval;

    void cacheStatusRequestFinished(QNetworkReply *reply);
    void downloadRequestFinished(QNetworkReply *reply);
    void getDataset();
    data_file_t loadDataFile(
            QStringList fileData, QDateTime lastModified, int fileSize,
            cache_stats_t cacheStats,
            QMap<enum DataFileWebTask::DataFileColumn, int> columnPositions);


    /** Map of column names used in data files to
     * column IDs we use here for faster lookups.
     */
    static QMap<QString, enum DataFileWebTask::DataFileColumn> labelColumns;
};

#endif // FETCHSAMPLESSUBTASKS_H

