#include "customisechartdialog.h"
#include "ui_customisechartdialog.h"

#include <QLabel>
#include <QStringList>
#include <QtDebug>

CustomiseChartDialog::CustomiseChartDialog(QMap<SampleColumn, GraphStyle> graphStyles, QWidget *parent) :
    QDialog(parent),
    ui(new Ui::CustomiseChartDialog)
{
    ui->setupUi(this);

    connect(ui->buttonBox, SIGNAL(accepted()), this, SLOT(AcceptDialog()));

    setGraphStyles(graphStyles);

    // setup the graph settings tab
    addGraphSettingsForColumn(SC_Temperature);
    addGraphSettingsForColumn(SC_ApparentTemperature);
    addGraphSettingsForColumn(SC_WindChill);
    addGraphSettingsForColumn(SC_DewPoint);
    addGraphSettingsForColumn(SC_IndoorTemperature);
    addGraphSettingsForColumn(SC_Humidity);
    addGraphSettingsForColumn(SC_IndoorHumidity);
    addGraphSettingsForColumn(SC_Pressure);
    addGraphSettingsForColumn(SC_Rainfall);
    addGraphSettingsForColumn(SC_AverageWindSpeed);
    addGraphSettingsForColumn(SC_GustWindSpeed);
    addGraphSettingsForColumn(SC_WindDirection);
}

CustomiseChartDialog::~CustomiseChartDialog()
{
    delete ui;
}

void CustomiseChartDialog::setGraphStyles(QMap<SampleColumn, GraphStyle> graphStyles) {
    this->graphStyles = graphStyles;
}

QMap<SampleColumn, GraphStyle> CustomiseChartDialog::getGraphStyles() {
    return graphStyles;
}

void CustomiseChartDialog::addGraphSettingsForColumn(SampleColumn column) {

    static int row = 2;

    GraphStyle gs(column);
    bool graphEnabled = graphStyles.contains(column);
    if (graphEnabled)
        gs = graphStyles[column];

    QLabel *graph = new QLabel(this);
    graph->setText(gs.getName());
    graph->setEnabled(graphEnabled);

    QLineEdit *graphName = new QLineEdit(this);
    graphName->setText(gs.getName());
    graphName->setEnabled(graphEnabled);

    QStringList lineStyles;
    lineStyles << tr("None")
               << tr("Line")
               << tr("Step Left")
               << tr("Step Right")
               << tr("Step Center")
               << tr("Impulse");

    QComboBox *lineStyle = new QComboBox(this);
    lineStyle->addItems(lineStyles);
    lineStyle->setEnabled(graphEnabled);

    switch(gs.getLineStyle()) {
    case QCPGraph::lsNone:
        lineStyle->setCurrentIndex(LS_None); break;
    case QCPGraph::lsStepLeft:
        lineStyle->setCurrentIndex(LS_StepLeft); break;
    case QCPGraph::lsStepRight:
        lineStyle->setCurrentIndex(LS_StepRight); break;
    case QCPGraph::lsStepCenter:
        lineStyle->setCurrentIndex(LS_StepCenter); break;
    case QCPGraph::lsImpulse:
        lineStyle->setCurrentIndex(LS_Impulse); break;
    case QCPGraph::lsLine:
    default:
        lineStyle->setCurrentIndex(LS_Line); break;
    }


    #define ADD_POINT_STYLE(name, label) pointStyle->addItem(QIcon(":/icons/scatter_style/" name), label)

    QComboBox *pointStyle = new QComboBox(this);
    pointStyle->addItem(tr("None"));
    pointStyle->addItem(tr("Dot"));
    ADD_POINT_STYLE("ssCross", tr("Cross"));
    ADD_POINT_STYLE("ssPlus", tr("Plus"));
    ADD_POINT_STYLE("ssCircle", tr("Circle"));
    ADD_POINT_STYLE("ssDisc", tr("Disc"));
    ADD_POINT_STYLE("ssSquare", tr("Square"));
    ADD_POINT_STYLE("ssDiamond", tr("Diamond"));
    ADD_POINT_STYLE("ssStar", tr("Star"));
    ADD_POINT_STYLE("ssTriangle", tr("Triangle"));
    ADD_POINT_STYLE("ssTriangleInverted", tr("Triangle (Inverted)"));
    ADD_POINT_STYLE("ssCrossSquare", tr("Cross Square"));
    ADD_POINT_STYLE("ssPlusSquare", tr("Plus Square"));
    ADD_POINT_STYLE("ssCrossCircle", tr("Cross Circle"));
    ADD_POINT_STYLE("ssPlusCircle", tr("Plus Circle"));
    pointStyle->setEnabled(graphEnabled);

    switch(gs.getScatterStyle().shape()) {
    case QCPScatterStyle::ssDot:
        pointStyle->setCurrentIndex(PS_Dot); break;
    case QCPScatterStyle::ssCross:
        pointStyle->setCurrentIndex(PS_Cross); break;
    case QCPScatterStyle::ssPlus:
        pointStyle->setCurrentIndex(PS_Plus); break;
    case QCPScatterStyle::ssCircle:
        pointStyle->setCurrentIndex(PS_Circle); break;
    case QCPScatterStyle::ssDisc:
        pointStyle->setCurrentIndex(PS_Disc); break;
    case QCPScatterStyle::ssSquare:
        pointStyle->setCurrentIndex(PS_Square); break;
    case QCPScatterStyle::ssDiamond:
        pointStyle->setCurrentIndex(PS_Diamond); break;
    case QCPScatterStyle::ssStar:
        pointStyle->setCurrentIndex(PS_Star); break;
    case QCPScatterStyle::ssTriangle:
        pointStyle->setCurrentIndex(PS_Triangle); break;
    case QCPScatterStyle::ssTriangleInverted:
        pointStyle->setCurrentIndex(PS_Triangle_Inverted); break;
    case QCPScatterStyle::ssCrossSquare:
        pointStyle->setCurrentIndex(PS_Cross_Square); break;
    case QCPScatterStyle::ssPlusSquare:
        pointStyle->setCurrentIndex(PS_Plus_Square); break;
    case QCPScatterStyle::ssCrossCircle:
        pointStyle->setCurrentIndex(PS_Cross_Circle); break;
    case QCPScatterStyle::ssPlusCircle:
        pointStyle->setCurrentIndex(PS_Plus_Circle); break;
    case QCPScatterStyle::ssNone:
    default:
        pointStyle->setCurrentIndex(PS_None); break;
    }

    QtColorButton *lineColour = new QtColorButton(this);
    lineColour->setColor(gs.getPen().color());
    lineColour->setMinimumSize(64, 23);
    lineColour->setMaximumSize(64, 23);
    lineColour->setEnabled(graphEnabled);

    ui->graphSettingsLayout->addWidget(graph, row, 0);
    ui->graphSettingsLayout->addWidget(graphName, row, 1);
    ui->graphSettingsLayout->addWidget(lineStyle, row, 2);
    ui->graphSettingsLayout->addWidget(pointStyle, row, 3);
    ui->graphSettingsLayout->addWidget(lineColour, row, 4);
    row++;

    GraphSettingsWidgets widgets;
    widgets.lineColour = lineColour;
    widgets.lineStyle = lineStyle;
    widgets.name = graphName;
    widgets.pointStyle = pointStyle;

    graphSettingsWidgets.insert(column, widgets);
}

void CustomiseChartDialog::updateGraphStyle(SampleColumn column) {
    GraphSettingsWidgets widgets = graphSettingsWidgets[column];
    qDebug() << "Updating graph style for column" << column;
    if (widgets.name->isEnabled()) {
        GraphStyle gs;
        gs.setName(widgets.name->text());
        gs.setLineColour(widgets.lineColour->color());

        switch(widgets.lineStyle->currentIndex()) {
        case LS_StepLeft:
            gs.setLineStyle(QCPGraph::lsStepLeft); break;
        case LS_StepRight:
            gs.setLineStyle(QCPGraph::lsStepRight); break;
        case LS_StepCenter:
            gs.setLineStyle(QCPGraph::lsStepCenter); break;
        case LS_Impulse:
            gs.setLineStyle(QCPGraph::lsImpulse); break;
        case LS_None:
            gs.setLineStyle(QCPGraph::lsNone); break;
        case LS_Line:
        default:
            gs.setLineStyle(QCPGraph::lsLine); break;
        }

        switch(widgets.pointStyle->currentIndex()) {
        case PS_Dot:
            gs.setScatterStyle(QCPScatterStyle::ssDot); break;
        case PS_Cross:
            gs.setScatterStyle(QCPScatterStyle::ssCross); break;
        case PS_Plus:
            gs.setScatterStyle(QCPScatterStyle::ssPlus); break;
        case PS_Circle:
            gs.setScatterStyle(QCPScatterStyle::ssCircle); break;
        case PS_Disc:
            gs.setScatterStyle(QCPScatterStyle::ssDisc); break;
        case PS_Square:
            gs.setScatterStyle(QCPScatterStyle::ssSquare); break;
        case PS_Diamond:
            gs.setScatterStyle(QCPScatterStyle::ssDiamond); break;
        case PS_Star:
            gs.setScatterStyle(QCPScatterStyle::ssStar); break;
        case PS_Triangle:
            gs.setScatterStyle(QCPScatterStyle::ssTriangle); break;
        case PS_Triangle_Inverted:
            gs.setScatterStyle(QCPScatterStyle::ssTriangleInverted); break;
        case PS_Cross_Square:
            gs.setScatterStyle(QCPScatterStyle::ssCrossSquare); break;
        case PS_Plus_Square:
            gs.setScatterStyle(QCPScatterStyle::ssPlusSquare); break;
        case PS_Cross_Circle:
            gs.setScatterStyle(QCPScatterStyle::ssCrossCircle); break;
        case PS_Plus_Circle:
            gs.setScatterStyle(QCPScatterStyle::ssPlusCircle); break;
        case PS_None:
        default:
            gs.setScatterStyle(QCPScatterStyle::ssNone);
        }

        graphStyles[column] = gs;
    }
}

void CustomiseChartDialog::AcceptDialog() {
    updateGraphStyle(SC_Temperature);
    updateGraphStyle(SC_ApparentTemperature);
    updateGraphStyle(SC_WindChill);
    updateGraphStyle(SC_DewPoint);
    updateGraphStyle(SC_IndoorTemperature);
    updateGraphStyle(SC_Humidity);
    updateGraphStyle(SC_IndoorHumidity);
    updateGraphStyle(SC_Pressure);
    updateGraphStyle(SC_Rainfall);
    updateGraphStyle(SC_AverageWindSpeed);
    updateGraphStyle(SC_GustWindSpeed);
    updateGraphStyle(SC_WindDirection);

    accept();
}
