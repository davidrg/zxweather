#ifndef CHARTSETTINGS_H
#define CHARTSETTINGS_H

#include <QMap>
#include <QString>

#include "graphstyle.h"
#include "datasource/samplecolumns.h"


struct ChartSettings {
    QMap<SampleColumn, GraphStyle> graphStyles;

    bool hasTitle;
    QString title;
};

#endif // CHARTSETTINGS_H
