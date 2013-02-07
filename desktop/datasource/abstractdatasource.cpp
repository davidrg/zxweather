#include "abstractdatasource.h"

AbstractDataSource::AbstractDataSource(QWidget *parentWidget, QObject *parent) :
    AbstractLiveDataSource(parent)
{
    progressDialog.reset(new QProgressDialog(parentWidget));
}
