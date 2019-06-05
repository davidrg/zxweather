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

#include <QApplication>
#include <QtDebug>
#include <QFile>
#include <QTextStream>
#include "mainwindow.h"

#define LOG_FILE "desktop.log"

#ifndef qInfo
#define qInfo qDebug
#endif

static QFile _global_log_file;
static QTextStream _log_stream;

#if (QT_VERSION >= QT_VERSION_CHECK(5,0,0))
void msgFileHandler(QtMsgType type, const QMessageLogContext &, const QString & msg)
#else
 void msgFileHandler(QtMsgType type, const char *msg)
#endif
{
    QString output_line;
    switch (type) {
    case QtDebugMsg:
        output_line = QString("Debug: %1").arg(msg);
        break;
#if (QT_VERSION >= QT_VERSION_CHECK(5,0,0))
    case QtInfoMsg:
        output_line = QString("Info: %1").arg(msg);
        break;
#endif
    case QtWarningMsg:
        output_line = QString("Warning: %1").arg(msg);
    break;
    case QtCriticalMsg:
        output_line = QString("Critical: %1").arg(msg);
    break;
    case QtFatalMsg:
        output_line = QString("Fatal: %1").arg(msg);
    break;
    }
    _log_stream << output_line << endl;
}

int main(int argc, char *argv[])
{    
    _global_log_file.setFileName(LOG_FILE);
    if (_global_log_file.exists() && _global_log_file.open(QIODevice::WriteOnly | QIODevice::Append)) {
        qInfo() << "Log file" << LOG_FILE << "found and opened successfully! Redirecting log output...";

        _log_stream.setDevice(&_global_log_file);

#if (QT_VERSION >= QT_VERSION_CHECK(5,0,0))
        qInstallMessageHandler(msgFileHandler);
#else
        qInstallMsgHandler(msgFileHandler);
#endif
    } else if (!_global_log_file.exists()) {
#ifdef Q_OS_WIN
        fprintf(stdout, "Log file " LOG_FILE " not found. Log output will be sent to the debugger.");
#endif
        qInfo() << "Log file" << LOG_FILE << "not found.";

    } else {
#ifdef Q_OS_WIN
        fprintf(stderr, "Failed to open log file " LOG_FILE " for write+append. Log output will be sent to the debugger.");
#endif
        qWarning() << "Failed to open log file" << LOG_FILE << "for write+append.";
    }

    QApplication a(argc, argv);

    QCoreApplication::setOrganizationName("zxnet");
    QCoreApplication::setOrganizationDomain("zx.net.nz");
    QCoreApplication::setApplicationName("zxweather-desktop");

    MainWindow w;
    w.adjustSize();
    w.show();

    return a.exec();
}
