#include "abstractdatasource.h"

AbstractDataSource::AbstractDataSource(QWidget *parentWidget, QObject *parent) :
    AbstractLiveDataSource2(parent)
{
    progressDialog.reset(new QProgressDialog(parentWidget));
}
