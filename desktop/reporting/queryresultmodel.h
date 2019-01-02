#ifndef QUERYRESULTMODEL_H
#define QUERYRESULTMODEL_H

#include <QAbstractTableModel>
#include <QStringList>

class QueryResultModel : public QAbstractTableModel {
    Q_OBJECT
public:
    QueryResultModel(QStringList columnNames, QList<QVariantList> rowData, QObject *parent = NULL);
    int rowCount(const QModelIndex &parent = QModelIndex()) const;
    int columnCount(const QModelIndex &parent = QModelIndex()) const;
    QVariant data(const QModelIndex &index, int role = Qt::DisplayRole) const;
    QVariant headerData(int section, Qt::Orientation orientation, int role = Qt::DisplayRole) const;
    bool setHeaderData(int section, Qt::Orientation orientation, const QVariant &value,
                                   int role = Qt::EditRole);
private:
    QStringList columnNames;
    QList<QVariantList> rowData;
};

#endif // QUERYRESULTMODEL_H
