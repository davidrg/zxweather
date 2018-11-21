#include "imagepropertiesdialog.h"
#include "ui_imagepropertiesdialog.h"
#include "json/json.h"

#include <QStringList>
#include <QLocale>

#define TS_FORMAT "ddd dd MMM yyyy HH:mm:ss"

QString toHumanSize(quint64 size) {
    double humanSize = size;
    QString humanSuffix = QString::null;
    if (humanSize > 1024) {
        humanSize /= 1024;
        humanSuffix = "KiB";
    }
    if (humanSize > 1024) {
        humanSize /= 1024;
        humanSuffix = "MiB";
    }
    if (humanSize > 1024) {
        humanSize /= 1024;
        humanSuffix = "GiB";
    }

    QLocale locale;

    if (humanSuffix.isNull()) {
        return QString("%1 bytes").arg(locale.toString(size));
    }
    return QString("%1 %2 (%3 bytes)").arg(QString::number(humanSize,'f', 2)).arg(humanSuffix).arg(locale.toString(size));
    //return QString("%1 %2 (%3 bytes)").arg(QString::number(humanSize,'f', 2)).arg(humanSuffix).arg(size);
}

QString toHumanTime(uint time) {
    QString seconds = QString::number(time) + " seconds";
    if (time <= 60) {
        return seconds;
    }

    QString hhmmss = QDateTime::fromTime_t(time).toUTC().toString("hh:mm:ss");

    return QString("%0 (%1)").arg(hhmmss).arg(seconds);
}

ImagePropertiesDialog::ImagePropertiesDialog(ImageInfo info, quint64 size,
                                             QImage image, QWidget *parent) :
    QDialog(parent),
    ui(new Ui::ImagePropertiesDialog)
{
    ui->setupUi(this);

    if (info.mimeType.startsWith("video/")) {
        ui->icon->setPixmap(QPixmap(":/icons/film-32"));
        setWindowIcon(QIcon(":/icons/film"));
        setWindowTitle("Video Properties");
    } else if (info.mimeType.startsWith("audio/")) {
        // TODO: get an audio file icon
        ui->icon->setPixmap(QPixmap(":/icons/audio-32"));
        setWindowIcon(QIcon(":/icons/audio"));
        setWindowTitle("Recording Properties");
    } else if (info.mimeType.startsWith("image/")) {
        ui->icon->setPixmap(QPixmap(":/icons/image-32"));
        setWindowIcon(QIcon(":/icons/image"));
        setWindowTitle("Image Properties");
    }

    ui->timeStamp->setText(info.timeStamp.toString(TS_FORMAT));
    ui->typeName->setText(info.imageTypeName);
    ui->sourceName->setText(info.imageSource.name);
    ui->mimeType->setText(info.mimeType);

    if (info.mimeType.startsWith("image/")) {
        ui->dimensions->setText(QString("%1x%2").arg(image.width()).arg(image.height()));
    } else {
        ui->dimensions->setVisible(false);
        ui->dimensionsLabel->setVisible(false);
    }

    ui->title->setText(info.title);
    ui->description->setText(info.description);
    ui->size->setText(toHumanSize(size));

    bool hasMetadata = !info.metadata.isNull() && info.hasMetadata;

    if (hasMetadata) {
        using namespace QtJson;

        bool ok;
        QVariant doc = Json::parse(info.metadata, ok);
        if (doc.canConvert<QVariantMap>()) {
            QVariantMap map = doc.toMap();

            // Handle a bunch of known metadata values in the desired display
            // order

            // Video metadata
            if (map.contains("start")) {
                addMetadata("Start", map["start"].toDateTime().toString(TS_FORMAT));
            }
            if (map.contains("finish")) {
                addMetadata("Finish", map["finish"].toDateTime().toString(TS_FORMAT));
            }
            if (map.contains("interval")) {
                addMetadata("Frame interval", map["interval"].toString() + " seconds");
            }
            if (map.contains("frame_count")) {
                addMetadata("Frame count", map["frame_count"]);
            }
            if (map.contains("frame_rate")) {
                addMetadata("Frame rate", map["frame_rate"]);
            }
            if (map.contains("processing_time")) {
                addMetadata("Encoding time", toHumanTime(map["processing_time"].toUInt()));
            }
            if (map.contains("total_size")) {
                addMetadata("Input size", toHumanSize(map["total_size"].toULongLong()));
            }

            // APT metadata
            if (map.contains("satellite")) {
                addMetadata("Satellite", map["satellite"]);
            }
            if (map.contains("aos_time")) {
                addMetadata("Signal acquisition",
                            QDateTime::fromTime_t(
                                map["aos_time"].toInt()).toString(TS_FORMAT));
            }
            if (map.contains("azimuth")) {
                addMetadata("Azimuth", map["azimuth"].toString() + "\xB0");
            }
            if (map.contains("direction")) {
                addMetadata("Direction", map["direction"]);
            }
            if (map.contains("max_elevation")) {
                addMetadata("Max elevation", map["max_elevation"].toString() + "\xB0");
            }
            if (map.contains("enhancement")) {
                addMetadata("Enhancement", map["enhancement"]);
            }
            if (map.contains("with_map")) {
                addMetadata("Map overlay", map["with_map"]);
            }
            if (map.contains("rec_len")) {
                addMetadata("Signal length", map["rec_len"]);
            }
            if (map.contains("frequency")) {
                addMetadata("Frequency", map["frequency"]);
            }
            if (map.contains("bandwidth")) {
                addMetadata("Bandwidth", map["bandwidth"]);
            }
            if (map.contains("duration")) {
                addMetadata("Duration", map["duration"]);
            }


            QStringList exclusions;
            exclusions << "start" << "finish" << "interval" << "frame_count"
                       << "frame_rate" << "processing_time" << "aos_time"
                       << "azimuth" << "direction" << "enhancement"
                       << "max_elevation" << "rec_len" << "satellite"
                       << "with_map" << "frequency" << "bandwidth"
                       << "duration" << "total_size";

            // Add in anything else thats in the document
            foreach (QString key, map.keys()) {
                if (!exclusions.contains(key)) {
                    addMetadata(key, map[key], 0);
                }
            }
        }
    }

    if (!hasMetadata || ui->metadataTree->topLevelItemCount() == 0) {
        ui->metadataTab->hide();
        ui->tabWidget->removeTab(1);
    }
}

void ImagePropertiesDialog::addMetadata(QString key, QVariant item, QTreeWidgetItem *parent) {
    if (item.canConvert<QVariantMap>()) {
        QStringList val;
        val.append(key);

        QTreeWidgetItem *treeItem;
        if (parent == 0)
            treeItem = new QTreeWidgetItem(val);
        else
            treeItem = new QTreeWidgetItem(parent, val);

        QVariantMap map = item.toMap();
        foreach (QString key, map.keys()) {
            addMetadata(key, map[key], treeItem);
        }
        if (parent == 0) {
            ui->metadataTree->addTopLevelItem(treeItem);
        }
    } else {
        QStringList val;
        val.append(key);
        val.append(item.toString());

        QTreeWidgetItem *treeItem;
        if (parent == 0)
            treeItem = new QTreeWidgetItem(val);
        else
            treeItem = new QTreeWidgetItem(parent, val);

        if (parent == 0) {
            ui->metadataTree->addTopLevelItem(treeItem);
        }
    }
}

ImagePropertiesDialog::~ImagePropertiesDialog()
{
    delete ui;
}
