#include "sortproxymodel.h"

#include <QStringList>
#include <QTime>

SortProxyModel::SortProxyModel(QObject *parent): QSortFilterProxyModel(parent)
{

}

bool SortProxyModel::lessThan(const QModelIndex &left,
                                      const QModelIndex &right) const
{
    QVariant leftData = sourceModel()->data(left);
    QVariant rightData = sourceModel()->data(right);

    // Handle null values. Null is considered to be a larger than any non-null value.
    // This is the default sort behaviour for PostgreSQL.
    bool leftIsNull = leftData.isNull();
    if (!leftIsNull) {
        leftIsNull = leftData.toString() == "--";
    }

    bool rightIsNull = rightData.isNull();
    if (!rightIsNull) {
        rightIsNull = rightData.toString() == "--";
    }

    if (leftIsNull && !rightIsNull) {
        return false; // We'll consider null greater than non-null values.
    }

    // If both values can be parsed as a float then sort as a float. This is to
    // handle SQL Queries that return numbers cast to strings (to maintain proper
    // formatting in output templates).
    bool leftIsFloat;
    float leftFloat = leftData.toFloat(&leftIsFloat);

    bool rightIsFloat;
    float rightFloat = rightData.toFloat(&rightIsFloat);

    if (leftIsFloat && rightIsFloat) {
        return leftFloat < rightFloat;
    }

    //See if either looks like SQL Interval with a days component
    // for example, "5 days 22:40:00"
    QStringList leftParts = leftData.toString().split(" ");
    QStringList rightParts = rightData.toString().split(" ");
    bool leftDays = leftParts.count() == 3 && leftParts.at(1) == "days";
    bool rightDays = rightParts.count() == 3 && rightParts.at(1) == "days";
    if (leftDays || rightDays) {
        int leftSeconds = -1, rightSeconds = -1;
        if (leftDays) {
            bool ok;
            int days = leftParts.at(0).toInt(&ok);
            if (ok) {
                leftSeconds = days * 86400; // Seconds in a day

                QTime t = QTime::fromString(leftParts.at(2), "hh:mm:ss");
                if (t.isValid()) {
                    leftSeconds += t.msecsSinceStartOfDay() / 1000;
                } else {
                    leftSeconds = -1;
                }
            }
        } else {
            QTime t = QTime::fromString(leftData.toString(), "hh:mm:ss");
            if (t.isValid()) {
                leftSeconds = t.msecsSinceStartOfDay() / 1000;
            }
        }

        if (rightDays) {
            bool ok;
            int days = rightParts.at(0).toInt(&ok);
            if (ok) {
                rightSeconds = days * 86400; // Seconds in a day

                QTime t = QTime::fromString(rightParts.at(2), "hh:mm:ss");
                if (t.isValid()) {
                    rightSeconds += t.msecsSinceStartOfDay() / 1000;
                } else {
                    rightSeconds = -1;
                }
            }
        } else {
            QTime t = QTime::fromString(rightData.toString(), "hh:mm:ss");
            if (t.isValid()) {
                rightSeconds = t.msecsSinceStartOfDay() / 1000;
            }
        }

        // We use -1 for parse failure. So only perform the comparison if both
        // the left and right sides are 0 or more.
        if (leftSeconds >= 0 && rightSeconds >= 0) {
            return leftSeconds < rightSeconds;
        }
    }

    // Otherwise we delegate to the default comparison
    return QSortFilterProxyModel::lessThan(left, right);
}
