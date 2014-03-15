#ifndef GRAPHSTYLE_H
#define GRAPHSTYLE_H

#include <Qstring>
#include <QPen>
#include <QBrush>
#include "charts/qcp/qcustomplot.h"
#include "datasource/samplecolumns.h"

class GraphStyle {
public:
    GraphStyle() {}
    GraphStyle(SampleColumn column);

    QString getName() const { return name; }
    QPen getPen() const { return pen; }
    QCPScatterStyle getScatterStyle() const { return scatterStyle; }
    QBrush getBrush() const { return brush; }
    QCPGraph::LineStyle getLineStyle() const { return lineStyle; }

    void setName(QString name) { this->name = name; }
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
};

#endif // GRAPHSTYLE_H
