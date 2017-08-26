#ifndef IMAGEPROPERTIESDIALOG_H
#define IMAGEPROPERTIESDIALOG_H

#include <QDialog>
#include <QVariantMap>
#include <QTreeWidgetItem>

#include "datasource/imageset.h"

namespace Ui {
class ImagePropertiesDialog;
}

class ImagePropertiesDialog : public QDialog
{
    Q_OBJECT

public:
    explicit ImagePropertiesDialog(ImageInfo info, quint64 size,
                                   QImage image,
                                   QWidget *parent = 0);
    ~ImagePropertiesDialog();

    void addMetadata(QString key, QVariant item, QTreeWidgetItem *parent=0);

private:
    Ui::ImagePropertiesDialog *ui;
};

#endif // IMAGEPROPERTIESDIALOG_H
