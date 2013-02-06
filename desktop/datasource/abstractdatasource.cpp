#include "abstractdatasource.h"

AbstractDataSource::AbstractDataSource(QWidget *parentWidget, QObject *parent) :
    QObject(parent)
{
    progressDialog.reset(new QProgressDialog(parentWidget));
}
