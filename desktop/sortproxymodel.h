#ifndef SORTPROXYMODEL_H
#define SORTPROXYMODEL_H

#include <QSortFilterProxyModel>

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

protected:
    virtual bool lessThan(const QModelIndex &source_left, const QModelIndex &source_right) const;
};

#endif // SORTPROXYMODEL_H
