#ifndef GRAPHSTYLE_H
#define GRAPHSTYLE_H

#include <QString>
#include <QPen>
#include <QBrush>
#include "charts/qcp/qcustomplot.h"
#include "datasource/samplecolumns.h"
#include "datasource/abstractlivedatasource.h"

class GraphStyle {
public:
    GraphStyle() {}
    GraphStyle(StandardColumn column);
    GraphStyle(ExtraColumn column);
    GraphStyle(LiveValue column);

    QString getName() const { return name; }
    QPen getPen() const { return pen; }
    QCPScatterStyle getScatterStyle() const { return scatterStyle; }
    QBrush getBrush() const { return brush; }
    QCPGraph::LineStyle getLineStyle() const { return lineStyle; }
    StandardColumn getColumnType() const {return column; }
    ExtraColumn getExtraColumnType() const {return extraColumn; }
    LiveValue getLiveColumnType() const {return liveColumn; }
    bool isLive() const { return isLiveColumn; }
    bool isExtraColumn() const {return isExtra; }
    QString getColumnName() const {return columnName; }
    QColor getDefaultColour() const {return defaultColour; }

    void setName(QString name);
    void setLineColour(QColor colour) { pen = QPen(colour); }
    void setScatterStyle(QCPScatterStyle style) { scatterStyle = style; }
    void setLineStyle(QCPGraph::LineStyle style) { lineStyle = style; }

    void applyStyle(QCPGraph* graph);

    bool operator==(GraphStyle& rhs) const;
    bool operator!=(GraphStyle& rhs) const;

private:
    QString name;
    QPen pen;
    QCPScatterStyle scatterStyle;
    QBrush brush;
    QCPGraph::LineStyle lineStyle;
    StandardColumn column;
    ExtraColumn extraColumn;
    LiveValue liveColumn;
    bool isLiveColumn;
    bool isExtra;
    QString columnName;
    QColor defaultColour;
};

#endif // GRAPHSTYLE_H
