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

#include <QtDebug>
#include <QFile>
#include <QTextStream>
#include <QApplication>
#include <QCommandLineParser>
#include <QCommandLineOption>
#include <QVariantList>
#include "mainwindow.h"
#include "settings.h"
#include "constants.h"
#include "json/json.h"

#ifdef SINGLE_INSTANCE
#include "applock.h"
#endif

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

    QCommandLineParser parser;
    parser.setApplicationDescription("zxweather desktop client");
    parser.addHelpOption();
    parser.addVersionOption();
    parser.addPositionalArgument("url", QCoreApplication::translate("main", "Open internal url"));

    QCommandLineOption stationCodeOption(
                QStringList() << "s" << "station-code",
                QCoreApplication::translate("main", "Override configured station code"));
    stationCodeOption.setValueName(QCoreApplication::translate("main", "station_code"));
    parser.addOption(stationCodeOption);

    QCommandLineOption configFileOption(
                QStringList() << "c" << "config-file",
                QCoreApplication::translate("main", "Configuration file"));
    configFileOption.setValueName(QCoreApplication::translate("main", "config_file"));
    parser.addOption(configFileOption);


    QCommandLineOption reportSearchPathAdd(
                QStringList() << "report-path-add",
                QCoreApplication::translate("main",
                                            "Add a directory to the report search path. If an instance is already running for the configured station this setting will be forwarded to that instance."));
    reportSearchPathAdd.setValueName(QCoreApplication::translate("main", "path"));
    parser.addOption(reportSearchPathAdd);

    QCommandLineOption reportSearchPathRemove(
                QStringList() << "report-path-remove",
                QCoreApplication::translate("main", "Remove a directory from the report search path. If an instance is already running for the configured station this setting will be forwarded to that instance."));
    reportSearchPathRemove.setValueName(QCoreApplication::translate("main", "path"));
    parser.addOption(reportSearchPathRemove);


    parser.process(a);

    if (parser.isSet(configFileOption)) {
        Settings::getInstance().setConfigFile(parser.value(configFileOption));
    }


    const QStringList args = parser.positionalArguments();
    qDebug() << "Arguments:" << args;

    using namespace QtJson;

    QVariantList positionalArgsList;
    foreach(QString arg, args) {
        positionalArgsList << arg;
    }

    QVariantList argsList;

    foreach(QString dir, parser.values(reportSearchPathAdd)) {
        QVariantMap m;
        m.insert("name", "reportPath+");
        m.insert("value", dir);
        argsList.append(m);
    }

    foreach(QString dir, parser.values(reportSearchPathRemove)) {
        QVariantMap m;
        m.insert("name", "reportPath-");
        m.insert("value", dir);
        argsList.append(m);
    }


    QVariantMap parametersMap;
    parametersMap.insert("positional", positionalArgsList);
    parametersMap.insert("args", argsList);

    QString message = Json::serialize(parametersMap);


    if (parser.isSet(stationCodeOption)) {
        Settings::getInstance().overrideStationCode(parser.value(stationCodeOption));
    }

#ifdef SINGLE_INSTANCE
    QString station_code = Settings::getInstance().stationCode();
    QString app_id = "zxw-desktop-" + station_code.toLower();

    AppLock lock;
    lock.lock(app_id);

    if (lock.isRunning()) {
        qDebug() << "Activating existing instance for station" << station_code << "with message" << message;

        return !lock.sendMessage(message);
    }
#endif

    QCoreApplication::setOrganizationName("zxnet");
    QCoreApplication::setOrganizationDomain("zx.net.nz");
    QCoreApplication::setApplicationName(Constants::APP_NAME);

    MainWindow w;

#ifdef SINGLE_INSTANCE
    lock.setWindow(&w);
    QObject::connect(&lock, SIGNAL(messageReceived(const QString &)),
                     &w, SLOT(messageReceived(const QString&)));
#endif

    w.adjustSize();
    w.show();

    w.messageReceived(message);

    return a.exec();
}
