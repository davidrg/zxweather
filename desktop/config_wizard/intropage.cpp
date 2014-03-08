#include "intropage.h"

#include "configwizard_private.h"

#include <QLabel>
#include <QVBoxLayout>

/****************************************************************************
********************************* INTRO PAGE ********************************
*****************************************************************************
* > Intro
*
* The first page of the wizard. Just contains a label saying what the
* wizard does.
*
****************************************************************************/




IntroPage::IntroPage(QWidget *parent)
    : QWizardPage(parent)
{
    setTitle(tr("Introduction"));

    setPixmap(QWizard::WatermarkPixmap, QPixmap(WATERMARK_PIXMAP));

    // TODO: make this first-run text instead.
    QLabel* topLabel = new QLabel(tr("<p>This wizard will guide you through the configuration process for the zxweather <i>desktop client</i>.</p>"
                             "<p>The desktop client enables to you to connect to a weather database on your network or an internet weather server and:"
                             "<ul>"
                             "<li>Receive live weather data</li>"
                             "<li>Produce custom charts</li>"
                             "<li>Export data</li>"
                             "</ul></p><p>Click next to continue.</p>"));
    topLabel->setWordWrap(true);

    QVBoxLayout *layout = new QVBoxLayout;
    layout->addWidget(topLabel);
    setLayout(layout);
}

int IntroPage::nextId() const {
    return ConfigWizard::Page_AccessType;
}
