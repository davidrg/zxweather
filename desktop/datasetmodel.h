#ifndef DATASETMODEL_H
#define DATASETMODEL_H

#include <QObject>
#include <QAbstractTableModel>

#include "datasource/sampleset.h"

#define DSM_SORT_ROLE (Qt::UserRole + 1)

class DataSetModel : public QAbstractTableModel
{
    Q_OBJECT

public:
    DataSetModel(DataSet dataSet, SampleSet sampleSet, QMap<ExtraColumn, QString> extraColumnNames, QObject *parent = 0);

    int rowCount(const QModelIndex &parent) const;
    int columnCount(const QModelIndex &parent) const;

    QVariant data(const QModelIndex &index, int role) const;
    QVariant headerData(int section, Qt::Orientation orientation,
                        int role) const;
private:
    DataSet dataSet;
    SampleSet sampleSet;

    QList<StandardColumn> getColumns();
    QList<ExtraColumn> getExtraColumns();

    QList<StandardColumn> columns;
    QList<ExtraColumn> extraColumns;

    QMap<ExtraColumn, QString> extraColumnNames;
};

#endif // DATASETMODEL_H
