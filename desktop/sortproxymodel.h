#ifndef SORTPROXYMODEL_H
#define SORTPROXYMODEL_H

#include <QSortFilterProxyModel>


// Comment this out to use a different interval parsing function.
// The regexp code is faster at rejecting non-matches but very slightly
// slower at parsing intervals.
//  >> Set in project file
//#define USE_INTERVAL_REGEXP

// Comment this out to disable additional sorting behaviours beyond
// the standard QSortFilterProxyModel sorting
#define ENABLE_EXTENDED_SORTING


#ifdef USE_INTERVAL_REGEXP
#include <QRegExp>
#endif

// A subclass of QSortFilterProxyModel with special handling for:
//  -> Null values and text values used to display null (--). Nulls are sorted
//     as being greater than all non-null values.
//  -> Numbers that have been cast to a string. This is often done in report
//     queries to ensure the number is rendered correctly.
//  -> Strings that look like a SQL Interval type ("d days hh:mm:ss" where d is
//     days, hh is hours, mm is minutes and ss is seconds)
class SortProxyModel : public QSortFilterProxyModel
{
public:
    SortProxyModel(QObject *parent = NULL);


    virtual void sort(int column, Qt::SortOrder order = Qt::AscendingOrder);

protected:
    virtual bool lessThan(const QModelIndex &source_left, const QModelIndex &source_right) const;

private:
#ifdef USE_INTERVAL_REGEXP
    QRegExp intervalRegExp;
#endif

    int intervalToSeconds(QString interval) const;
};

#endif // SORTPROXYMODEL_H
