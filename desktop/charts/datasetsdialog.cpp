#include "datasetsdialog.h"
#include "ui_datasetsdialog.h"

#define COL_DS 1
#define COL_AXIS 2

DataSetsDialog::DataSetsDialog(QList<DataSet> ds,
                               QMap<dataset_id_t, QString> names,
                               QMap<dataset_id_t, bool> axisVisibility,
                               QMap<dataset_id_t, bool> visibility,
                               QWidget *parent) :
    QDialog(parent),
    ui(new Ui::DataSetsDialog)
{
    ui->setupUi(this);

    connect(ui->pbAdd, SIGNAL(clicked(bool)), this, SLOT(addDataSetRequested()));
    connect(ui->twDataSets, SIGNAL(itemChanged(QTreeWidgetItem*,int)),
            this, SLOT(itemChanged(QTreeWidgetItem*,int)));
    connect(ui->twDataSets, SIGNAL(currentItemChanged(QTreeWidgetItem*,QTreeWidgetItem*)),
            this, SLOT(currentItemChanged(QTreeWidgetItem*,QTreeWidgetItem*)));

    foreach (DataSet s, ds) {
        QString name;
        if (names.contains(s.id)) {
            name = names[s.id];
        }

        bool axis = true;
        if (axisVisibility.contains(s.id)) {
            axis = axisVisibility[s.id];
        }

        bool visible = true;
        if (visibility.contains(s.id)) {
            visible = visibility[s.id];
        }

        addDataSetToUI(s, name, axis, visible);
    }

#if QT_VERSION >= 0x050000
    for (int c = 0; c < ui->twDataSets->header()->count(); ++c)
    {
        ui->twDataSets->header()->setSectionResizeMode(
            c, QHeaderView::ResizeToContents);
    }
#endif
}

void DataSetsDialog::addDataSetToUI(DataSet s, QString name, bool axisVisible, bool isVisible) {
    QTreeWidgetItem *twi = new QTreeWidgetItem();

    twi->setData(0, Qt::UserRole, s.id);
    twi->setText(0, name);

    twi->setCheckState(COL_DS, isVisible ? Qt::Checked : Qt::Unchecked);
    twi->setCheckState(COL_AXIS, axisVisible ? Qt::Checked : Qt::Unchecked);
    twi->setText(3, s.startTime.toString());
    twi->setText(4, s.endTime.toString());
    switch(s.aggregateFunction) {
    case AF_None:
        twi->setText(5, tr("None"));
        break;
    case AF_Average:
        twi->setText(5, tr("Average"));
        break;
    case AF_Minimum:
        twi->setText(5, tr("Minimum"));
        break;
    case AF_Maximum:
        twi->setText(5, tr("Maximum"));
        break;
    case AF_Sum:
        twi->setText(5, tr("Sum"));
        break;
    case AF_RunningTotal:
        twi->setText(5, tr("Running Total"));
        break;
    default:
        twi->setText(5, tr("Error"));
    }
    switch (s.groupType) {
    case AGT_None:
        twi->setText(6, tr("None"));
        break;
    case AGT_Month:
        twi->setText(6, tr("Month"));
        break;
    case AGT_Year:
        twi->setText(6, tr("Year"));
        break;
    case AGT_Custom:
        twi->setText(6, tr("Custom (%0 minutes)").arg(s.customGroupMinutes));
    }
    ui->twDataSets->addTopLevelItem(twi);
}

DataSetsDialog::~DataSetsDialog()
{
    delete ui;
}


void DataSetsDialog::addDataSetRequested() {
    emit addDataSet();
}

void DataSetsDialog::itemChanged(QTreeWidgetItem *twi, int column) {
    int id = twi->data(0, Qt::UserRole).toInt();

    if (column == COL_AXIS) {
        emit axisVisibilityChanged(id, twi->checkState(COL_AXIS) == Qt::Checked);
    } else if (column == COL_DS) {
        emit dataSourceVisibilityChanged(id, twi->checkState(COL_DS) == Qt::Checked);
    }
}

void DataSetsDialog::axisVisibilityChangedForDataSet(dataset_id_t dsId, bool visible) {
    for(int i = 0; i < ui->twDataSets->topLevelItemCount(); i++) {
        QTreeWidgetItem *twi = ui->twDataSets->topLevelItem(i);
        int id = twi->data(0, Qt::UserRole).toInt();
        if (id == dsId) {
            twi->setCheckState(COL_AXIS, visible ? Qt::Checked : Qt::Unchecked);
        }
    }
}

void DataSetsDialog::visibilityChangedForDataSet(dataset_id_t dsId, bool visible) {
    for(int i = 0; i < ui->twDataSets->topLevelItemCount(); i++) {
        QTreeWidgetItem *twi = ui->twDataSets->topLevelItem(i);
        int id = twi->data(0, Qt::UserRole).toInt();
        if (id == dsId) {
            twi->setCheckState(COL_DS, visible ? Qt::Checked : Qt::Unchecked);
        }
    }
}

void DataSetsDialog::currentItemChanged(QTreeWidgetItem* twi,QTreeWidgetItem* twiOld) {
    Q_UNUSED(twiOld);

    int id = twi->data(0, Qt::UserRole).toInt();

    emit dataSetSelected(id);
}

void DataSetsDialog::dataSetAdded(DataSet ds, QString name) {
    addDataSetToUI(ds, name, true, true);
}

void DataSetsDialog::dataSetRemoved(dataset_id_t dsId) {
    for(int i = 0; i < ui->twDataSets->topLevelItemCount(); i++) {
        QTreeWidgetItem *twi = ui->twDataSets->topLevelItem(i);
        int id = twi->data(0, Qt::UserRole).toInt();
        if (id == dsId) {
            ui->twDataSets->takeTopLevelItem(ui->twDataSets->indexOfTopLevelItem(twi));
            delete twi;
        }
    }
}
