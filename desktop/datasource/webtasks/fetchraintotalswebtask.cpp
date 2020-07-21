#include "fetchraintotalswebtask.h"
#include "json/json.h"

#include <QtDebug>

#define DATASET_RAIN_SUMMARY "rain_summary.json"

FetchRainTotalsWebTask::FetchRainTotalsWebTask(QString baseUrl,
                                               QString stationCode,
                                               WebDataSource* ds)
    : AbstractWebTask(baseUrl, stationCode, ds)
{

}

void FetchRainTotalsWebTask::beginTask() {
    QString url = _stationBaseUrl + DATASET_RAIN_SUMMARY;

    QNetworkRequest request(url);

    emit httpGet(request);
}

void FetchRainTotalsWebTask::networkReplyReceived(QNetworkReply *reply) {
    reply->deleteLater();

    if (reply->error() != QNetworkReply::NoError) {
        emit failed(reply->errorString());
    } else {
        QByteArray replyData = reply->readAll();

        using namespace QtJson;

        bool ok;
        QVariantMap result = Json::parse(replyData, ok).toMap();

        if (!ok) {
            qDebug() << "rain summary parse error. Data:" << replyData;
            emit failed(tr("JSON parsing failed while loading rain summary."));
            return;
        }

        QVariantMap day = result.value("today").toMap();
        QVariantMap month = result.value("this_month").toMap();
        QVariantMap year = result.value("this_year").toMap();
        // Other ranges available: yesterday, this_week

        QDate date = day.value("start").toDateTime().date();
        double day_total = day.value("total").toDouble();
        double month_total = month.value("total").toDouble();
        double year_total = year.value("total").toDouble();

        _dataSource->fireRainTotals(date, day_total, month_total, year_total);

        emit finished();
    }
}
