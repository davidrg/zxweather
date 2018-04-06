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
    GraphStyle(SampleColumn column);
    GraphStyle(LiveValue column);

    QString getName() const { return name; }
    QPen getPen() const { return pen; }
    QCPScatterStyle getScatterStyle() const { return scatterStyle; }
    QBrush getBrush() const { return brush; }
    QCPGraph::LineStyle getLineStyle() const { return lineStyle; }
    SampleColumn getColumnType() const {return column; }
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
    SampleColumn column;
    LiveValue liveColumn;
    QString columnName;
    QColor defaultColour;
};

#endif // GRAPHSTYLE_H
