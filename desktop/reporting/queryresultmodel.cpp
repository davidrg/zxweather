#include "queryresultmodel.h"


QueryResultModel::QueryResultModel(QStringList columnNames, QList<QVariantList> rowData, QObject *parent) : QAbstractTableModel (parent) {
    this->columnNames = columnNames;
    this->rowData = rowData;
}

int QueryResultModel::rowCount(const QModelIndex &parent) const {
    if (parent.isValid())
        return 0;
    return rowData.count();
}

int QueryResultModel::columnCount(const QModelIndex &parent) const {
    if (parent.isValid() || rowData.isEmpty())
        return 0;

    return columnNames.count();
}

QVariant QueryResultModel::data(const QModelIndex &index, int role) const {
    if (!index.isValid()) {
        return QVariant();
    }

    if (index.column() > columnCount() || index.row() > rowCount()) {
        return QVariant();
    }

    if (role == Qt::DisplayRole) {
        return this->rowData[index.row()][index.column()];
    }

    return QVariant();
}

QVariant QueryResultModel::headerData(int section, Qt::Orientation orientation, int role) const {
    if (orientation == Qt::Horizontal && role == Qt::DisplayRole) {
        return this->columnNames[section];
    }

    return QAbstractTableModel::headerData(section, orientation, role);
}

bool QueryResultModel::setHeaderData(int section, Qt::Orientation orientation,
                                     const QVariant &value,
                                     int role) {
    if (orientation == Qt::Horizontal && role == Qt::DisplayRole) {
        this->columnNames[section] = value.toString();
        emit headerDataChanged(orientation, section, section);
        return true;
    }

    return false;
}

