#include "sortproxymodel.h"

#include <QStringList>
#include <QTime>
#include <QtDebug>

SortProxyModel::SortProxyModel(QObject *parent): QSortFilterProxyModel(parent)
{
#ifdef USE_INTERVAL_REGEXP
    // This regexp has two captures:
    //  1: number of days
    //  2: time component (hh:mm:ss)
    intervalRegExp = QRegExp("(?:(\\d+) days )?(\\d\\d:\\d\\d:\\d\\d)");
#endif
}

void SortProxyModel::sort(int column, Qt::SortOrder order) {
    int start = QDateTime::currentMSecsSinceEpoch();

    QSortFilterProxyModel::sort(column, order);

    int end = QDateTime::currentMSecsSinceEpoch();

    qDebug() << "Sort completed in" << end - start << "ms";
}

#ifdef USE_INTERVAL_REGEXP
int SortProxyModel::intervalToSeconds(QString interval) const {
    QTime time;
    int days = -1;
    if (intervalRegExp.indexIn(interval.trimmed()) == 0) {
        QString daysString = intervalRegExp.cap(1);
        if (daysString.isNull() || daysString.isEmpty()) {
            days = 0;
        } else {
            bool ok;
            days = daysString.toInt(&ok);
            if (!ok) {
                days = -1;
            }
        }
        if (days >= 0) {
            time = QTime::fromString(intervalRegExp.cap(2), "hh:mm:ss");
        }
    }

    if (days < 0 || !time.isValid() || time.isNull()) {
        return -1;
    }

    // 86400 seconds in a day, 1000ms in a second
    return (days * 86400) + (time.msecsSinceStartOfDay() / 1000);
}
#else
int SortProxyModel::intervalToSeconds(QString interval) const {
    QStringList parts = interval.trimmed().split(" ");

    bool hasDays = parts.count() == 3 && parts.at(1) == "days";
    int seconds = -1;

    if (hasDays) {
        bool ok;
        int days = parts.at(0).toInt(&ok);
        if (ok) {
            seconds = days * 86400; // Seconds in a day

            QTime t = QTime::fromString(parts.at(2), "hh:mm:ss");
            if (t.isValid()) {
                seconds += t.msecsSinceStartOfDay() / 1000;
            } else {
                seconds = -1;
            }
        }
    } else {
        QTime t = QTime::fromString(interval.trimmed(), "hh:mm:ss");
        if (t.isValid()) {
            seconds = t.msecsSinceStartOfDay() / 1000;
        }
    }

    return seconds;
}
#endif



bool SortProxyModel::lessThan(const QModelIndex &left,
                                      const QModelIndex &right) const
{
#ifdef ENABLE_EXTENDED_SORTING
    QVariant leftData = sourceModel()->data(left, sortRole());
    QVariant rightData = sourceModel()->data(right, sortRole());

    // Handle null values. Null is considered to be a larger than any non-null value.
    // This is the default sort behaviour for PostgreSQL.
    if (leftData.isNull() && !rightData.isNull()) {
        return false; // We'll consider null greater than non-null values.
    } else if (!leftData.isNull() && rightData.isNull()) {
        return true;
    }

    if (leftData.type() == QVariant::String || rightData.type() == QVariant::String) {
        QString leftString = leftData.toString();
        QString rightString = rightData.toString();

        if (sortRole() == Qt::DisplayRole) {
            // A string of "--" is used to signify null.
            bool leftIsNull = leftString == "--";
            bool rightIsNull = rightString == "--";

            // null < 5 == false
            // 5 < null == true
            if (leftIsNull && !rightIsNull) {
                return false;
            }
            if (!leftIsNull && rightIsNull) {
                return true;
            }
        }


        if (leftData.type() == QVariant::String && rightData.type() == QVariant::String) {
            // If both values are strings and can be parsed as floats then sort them as
            // floats. This is to handle SQL Queries that return numbres as strings to
            // maintain rounding and formatting when the values go into report templates.
            bool leftIsFloat;
            float leftFloat = leftData.toFloat(&leftIsFloat);

            if (leftIsFloat) {
                bool rightIsFloat;
                float rightFloat = rightData.toFloat(&rightIsFloat);

                if (rightIsFloat) {
                    return leftFloat < rightFloat;
                }
            }

            // Otherwise, try parsing them as intervals (x days hh:mm:ss)
            int leftSeconds = intervalToSeconds(leftString);

            if (leftSeconds >= 0) {
                int rightSeconds = intervalToSeconds(rightString);

                if (rightSeconds >= 0) {
                    // Managed to parse both sides as a time interval.
                    return leftSeconds < rightSeconds;
                }
            }
        }
    }

#endif
    // Otherwise we delegate to the default comparison
    return QSortFilterProxyModel::lessThan(left, right);
}
