#include "graphstyledialog.h"
#include "ui_graphstyledialog.h"
#include "graphstyle.h"
#include "settings.h"

#include <QPushButton>

GraphStyleDialog::GraphStyleDialog(GraphStyle &graphStyle, QWidget *parent) :
    QDialog(parent),
    ui(new Ui::GraphStyleDialog),
    style(graphStyle)
{
    ui->setupUi(this);

    connect(ui->buttonBox, SIGNAL(accepted()), this, SLOT(acceptDialog()));

    QPushButton *resetButton = ui->buttonBox->button(QDialogButtonBox::RestoreDefaults);
    connect(resetButton, SIGNAL(pressed()), this, SLOT(restoreDefaults()));

    ui->lblColumnType->setText(graphStyle.getColumnName());
    ui->leName->setText(graphStyle.getName());

    switch(graphStyle.getLineStyle()) {
    case QCPGraph::lsNone:
        ui->cbLineStyle->setCurrentIndex(LS_None); break;
    case QCPGraph::lsStepLeft:
        ui->cbLineStyle->setCurrentIndex(LS_StepLeft); break;
    case QCPGraph::lsStepRight:
        ui->cbLineStyle->setCurrentIndex(LS_StepRight); break;
    case QCPGraph::lsStepCenter:
        ui->cbLineStyle->setCurrentIndex(LS_StepCenter); break;
    case QCPGraph::lsImpulse:
        ui->cbLineStyle->setCurrentIndex(LS_Impulse); break;
    case QCPGraph::lsLine:
    default:
        ui->cbLineStyle->setCurrentIndex(LS_Line); break;
    }

    switch(graphStyle.getScatterStyle().shape()) {
    case QCPScatterStyle::ssDot:
        ui->cbPointStyle->setCurrentIndex(PS_Dot); break;
    case QCPScatterStyle::ssCross:
        ui->cbPointStyle->setCurrentIndex(PS_Cross); break;
    case QCPScatterStyle::ssPlus:
        ui->cbPointStyle->setCurrentIndex(PS_Plus); break;
    case QCPScatterStyle::ssCircle:
        ui->cbPointStyle->setCurrentIndex(PS_Circle); break;
    case QCPScatterStyle::ssDisc:
        ui->cbPointStyle->setCurrentIndex(PS_Disc); break;
    case QCPScatterStyle::ssSquare:
        ui->cbPointStyle->setCurrentIndex(PS_Square); break;
    case QCPScatterStyle::ssDiamond:
        ui->cbPointStyle->setCurrentIndex(PS_Diamond); break;
    case QCPScatterStyle::ssStar:
        ui->cbPointStyle->setCurrentIndex(PS_Star); break;
    case QCPScatterStyle::ssTriangle:
        ui->cbPointStyle->setCurrentIndex(PS_Triangle); break;
    case QCPScatterStyle::ssTriangleInverted:
        ui->cbPointStyle->setCurrentIndex(PS_Triangle_Inverted); break;
    case QCPScatterStyle::ssCrossSquare:
        ui->cbPointStyle->setCurrentIndex(PS_Cross_Square); break;
    case QCPScatterStyle::ssPlusSquare:
        ui->cbPointStyle->setCurrentIndex(PS_Plus_Square); break;
    case QCPScatterStyle::ssCrossCircle:
        ui->cbPointStyle->setCurrentIndex(PS_Cross_Circle); break;
    case QCPScatterStyle::ssPlusCircle:
        ui->cbPointStyle->setCurrentIndex(PS_Plus_Circle); break;
    case QCPScatterStyle::ssNone:
    default:
        ui->cbPointStyle->setCurrentIndex(PS_None); break;
    }

    ui->clrLineColour->setColor(graphStyle.getPen().color());
}

GraphStyleDialog::~GraphStyleDialog()
{
    delete ui;
}

void GraphStyleDialog::restoreDefaults() {
    ui->leName->setText(style.getColumnName());
    ui->cbLineStyle->setCurrentIndex(LS_Line);
    ui->cbPointStyle->setCurrentIndex(PS_None);
    ui->clrLineColour->setColor(style.getDefaultColour());
}

void GraphStyleDialog::acceptDialog() {

    style.setName(ui->leName->text());
    style.setLineColour(ui->clrLineColour->color());

    switch(ui->cbLineStyle->currentIndex()) {
    case LS_StepLeft:
        style.setLineStyle(QCPGraph::lsStepLeft); break;
    case LS_StepRight:
        style.setLineStyle(QCPGraph::lsStepRight); break;
    case LS_StepCenter:
        style.setLineStyle(QCPGraph::lsStepCenter); break;
    case LS_Impulse:
        style.setLineStyle(QCPGraph::lsImpulse); break;
    case LS_None:
        style.setLineStyle(QCPGraph::lsNone); break;
    case LS_Line:
    default:
        style.setLineStyle(QCPGraph::lsLine); break;
    }

    switch(ui->cbPointStyle->currentIndex()) {
    case PS_Dot:
        style.setScatterStyle(QCPScatterStyle::ssDot); break;
    case PS_Cross:
        style.setScatterStyle(QCPScatterStyle::ssCross); break;
    case PS_Plus:
        style.setScatterStyle(QCPScatterStyle::ssPlus); break;
    case PS_Circle:
        style.setScatterStyle(QCPScatterStyle::ssCircle); break;
    case PS_Disc:
        style.setScatterStyle(QCPScatterStyle::ssDisc); break;
    case PS_Square:
        style.setScatterStyle(QCPScatterStyle::ssSquare); break;
    case PS_Diamond:
        style.setScatterStyle(QCPScatterStyle::ssDiamond); break;
    case PS_Star:
        style.setScatterStyle(QCPScatterStyle::ssStar); break;
    case PS_Triangle:
        style.setScatterStyle(QCPScatterStyle::ssTriangle); break;
    case PS_Triangle_Inverted:
        style.setScatterStyle(QCPScatterStyle::ssTriangleInverted); break;
    case PS_Cross_Square:
        style.setScatterStyle(QCPScatterStyle::ssCrossSquare); break;
    case PS_Plus_Square:
        style.setScatterStyle(QCPScatterStyle::ssPlusSquare); break;
    case PS_Cross_Circle:
        style.setScatterStyle(QCPScatterStyle::ssCrossCircle); break;
    case PS_Plus_Circle:
        style.setScatterStyle(QCPScatterStyle::ssPlusCircle); break;
    case PS_None:
    default:
        style.setScatterStyle(QCPScatterStyle::ssNone);
    }

    accept();
}
