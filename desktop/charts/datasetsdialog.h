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
    void dataSetVisibilityChanged(dataset_id_t dsId, bool visible);
    void dataSetSelected(dataset_id_t dsId);
    void dataSetNameChanged(dataset_id_t dsId, QString name);
    void addGraph(dataset_id_t dsId);
    void changeTimeSpan(dataset_id_t dsId);
    void removeDataSet(dataset_id_t dsId);

public slots:
    void axisVisibilityChangedForDataSet(dataset_id_t dsId, bool visible);
    void visibilityChangedForDataSet(dataset_id_t dsId, bool visible);
    void dataSetAdded(DataSet ds, QString name);
    void dataSetRemoved(dataset_id_t dsId);
    void dataSetRenamed(dataset_id_t dsId, QString name);
    void dataSetTimeSpanChanged(dataset_id_t dsId, QDateTime start, QDateTime end);

private slots:
    void addDataSetRequested();
    void itemChanged(QTreeWidgetItem *twi, int column);
    void currentItemChanged(QTreeWidgetItem* twi,QTreeWidgetItem* twiOld);
    void contextMenuRequested(QPoint point);
    void doRename();
    void doAddGraph();
    void doChangeTimespan();
    void doRemove();


private:
    Ui::DataSetsDialog *ui;

    void addDataSetToUI(DataSet s, QString name, bool axisVisible, bool isVisible);
};

#endif // DATASETSDIALOG_H
