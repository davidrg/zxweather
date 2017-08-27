#include "abstractwebtask.h"

#include <QtDebug>

// Undefine this to use the tab delimited data files under /data
// (eg /data/sb/2016/2/samples.txt) instead of the files weather_plot
// generates for gnuplots use (eg /b/sb/2016/february/gnuplot_data.dat).
// This allows the use of the desktop client remotely without weather_plot
// running. Its incompatible with versions of zxweather < 1.0

// Uncomment this to use the tab delimited data files generated by
// weather_plot for gnuplots use (eg /b/sb/2016/february/gnuplot_data.dat)
// instead of the data files generated by zxw_web under /data
// (eg /data/sb/2016/2/samples.txt)
//#define USE_GNUPLOT_DATA
// Currently we use GNUPLOT data because the Web UI takes too long to prepare
// the required cache control headers. Some time this needs to be turned into a
// UI option or the Web UI needs to come up with the headers without getting the
// database involved.

AbstractWebTask::AbstractWebTask(QString baseUrl, QString stationCode,
                                 WebDataSource *ds)
    : QObject(ds)
{
    _baseUrl = baseUrl;
    _stationCode = stationCode.toLower();
    _dataRootUrl = _baseUrl + "data/";
    _stationBaseUrl = _dataRootUrl + stationCode + "/";
#ifdef USE_GNUPLOT_DATA
    _stationDataUrl = _baseUrl + "b/" + stationCode + "/";
#else
    _stationDataUrl = _stationBaseUrl;
#endif
    _dataSource = ds;


}
