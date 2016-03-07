#ifndef DATASETMODEL_H
#define DATASETMODEL_H

#include <QObject>
#include <QAbstractTableModel>

#include "datasource/sampleset.h"

class DataSetModel : public QAbstractTableModel
{
    Q_OBJECT

public:
    DataSetModel(DataSet dataSet, SampleSet sampleSet, QObject *parent = 0);

    int rowCount(const QModelIndex &parent) const;
    int columnCount(const QModelIndex &parent) const;

    QVariant data(const QModelIndex &index, int role) const;
    QVariant headerData(int section, Qt::Orientation orientation,
                        int role) const;
private:
    DataSet dataSet;
    SampleSet sampleSet;

    QList<SampleColumn> getColumns();

    QList<SampleColumn> columns;
};

#endif // DATASETMODEL_H
