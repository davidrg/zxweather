/*****************************************************************************
 *            Created: 23/06/2012
 *          Copyright: (C) Copyright David Goodwin, 2012
 *            License: GNU General Public License
 *****************************************************************************
 *
 *   This is free software; you can redistribute it and/or modify
 *   it under the terms of the GNU General Public License as published by
 *   the Free Software Foundation; either version 2 of the License, or
 *   (at your option) any later version.
 *
 *   This software is distributed in the hope that it will be useful,
 *   but WITHOUT ANY WARRANTY; without even the implied warranty of
 *   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *   GNU General Public License for more details.
 *
 *   You should have received a copy of the GNU General Public License
 *   along with this software; if not, write to the Free Software
 *   Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA  02110-1301  USA
 *
 ****************************************************************************/

#include "aboutdialog.h"
#include "ui_aboutdialog.h"
#include "constants.h"
#include "reporting/reportdisplaywindow.h"

AboutDialog::AboutDialog(QWidget *parent) :
    QDialog(parent),
    ui(new Ui::AboutDialog)
{
    ui->setupUi(this);
    this->setWindowFlags(this->windowFlags() & ~Qt::WindowContextHelpButtonHint);
    ui->textBrowser->setHtml(
                ui->textBrowser->document()->toHtml().replace(
                    "{version_str}", Constants::VERSION_STR)
                .replace("{copyright_year}", QString::number(COPYRIGHT_YEAR)));

    connect(ui->pbLicenses, SIGNAL(clicked(bool)), this, SLOT(showLicenses()));
}

AboutDialog::~AboutDialog()
{
    delete ui;
}

void AboutDialog::changeEvent(QEvent *e)
{
    QDialog::changeEvent(e);
    switch (e->type()) {
    case QEvent::LanguageChange:
        ui->retranslateUi(this);
        break;
    default:
        break;
    }
}

void AboutDialog::showLicenses() {
    ReportDisplayWindow *w = new ReportDisplayWindow("Licenses", QIcon(":/icons/about"), this);
    QFile gpl3(":/licenses/gpl_v3.txt");
    gpl3.open(QIODevice::ReadOnly);

    QFile lic(":/licenses/qtcolorbutton.txt");
    lic.open(QIODevice::ReadOnly);
    QFile lgpl21(":/licenses/lgpl-2.1.txt");
    lgpl21.open(QIODevice::ReadOnly);
    QFile digia_exception(":/licenses/lgpl_exception.txt");
    digia_exception.open(QIODevice::ReadOnly);

    QString license = "QtColorButton license (part of QtCreator)\n\n"
            + QString::fromUtf8(lic.readAll())
            + "\n\n"
            + QString::fromUtf8(lgpl21.readAll())
            + "\n\nLGPL_EXCEPTION.txt\n"
            + QString::fromUtf8(digia_exception.readAll());

    QFile mustache(":/licenses/mustache_bsd_2cl.txt");
    mustache.open(QIODevice::ReadOnly);
    QFile qtjson(":/licenses/qtjson_license.txt");
    qtjson.open(QIODevice::ReadOnly);

    w->addPlainTab("GPL v3", QIcon(), QString::fromUtf8(gpl3.readAll()));
    w->addPlainTab("QtColorButton", QIcon(), license);
    w->addPlainTab("qt-mustache", QIcon(), QString::fromUtf8(mustache.readAll()));
    w->addPlainTab("qt-json", QIcon(), QString::fromUtf8(qtjson.readAll()));

    w->setAttribute(Qt::WA_DeleteOnClose);
    w->show();
}
