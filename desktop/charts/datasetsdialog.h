#ifndef DATASETSDIALOG_H
#define DATASETSDIALOG_H

#include <QDialog>
#include <QMap>

#include "datasource/samplecolumns.h"

class QTreeWidgetItem;

namespace Ui {
class DataSetsDialog;
}

class DataSetsDialog : public QDialog
{
    Q_OBJECT

public:
    explicit DataSetsDialog(QList<DataSet> ds, QMap<dataset_id_t, QString> names,
                            QMap<dataset_id_t, bool> axisVisibility,
                            QMap<dataset_id_t, bool> visibility,
                            QWidget *parent = 0);
    ~DataSetsDialog();

signals:
    void addDataSet();
    void axisVisibilityChanged(dataset_id_t dsId, bool visible);
    void dataSourceVisibilityChanged(dataset_id_t dsId, bool visible);
    void dataSetSelected(dataset_id_t dsId);

public slots:
    void axisVisibilityChangedForDataSet(dataset_id_t dsId, bool visible);
    void visibilityChangedForDataSet(dataset_id_t dsId, bool visible);
    void dataSetAdded(DataSet ds, QString name);
    void dataSetRemoved(dataset_id_t dsId);

private slots:
    void addDataSetRequested();
    void itemChanged(QTreeWidgetItem *twi, int column);
    void currentItemChanged(QTreeWidgetItem* twi,QTreeWidgetItem* twiOld);

private:
    Ui::DataSetsDialog *ui;

    void addDataSetToUI(DataSet s, QString name, bool axisVisible, bool isVisible);
};

#endif // DATASETSDIALOG_H
