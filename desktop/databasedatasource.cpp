/*****************************************************************************
 *            Created: 25/08/2012
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

#include "databasedatasource.h"
#include "database.h"
#include "databaselivedata.h"

#include <QTimer>
#include <QDebug>

DatabaseLiveDataSource::DatabaseLiveDataSource(QString databaseName,
        QString hostname,
        int port,
        QString username,
        QString password,
        QString station,
        QObject *parent) :
    AbstractLiveDataSource(parent)
{
    // No wild pointers allowed
    notificationTimer = NULL;
    signalAdapter = NULL;

    // Continue with object setup...
    signalAdapter = new DBSignalAdapter(this);
    wdb_set_signal_adapter(signalAdapter);

    // Signals we won't handle specially:
    connect(signalAdapter, SIGNAL(connection_exception(QString)),
            this, SLOT(unknown_db_error(QString)));
    connect(signalAdapter, SIGNAL(connection_does_not_exist(QString)),
            this, SLOT(unknown_db_error(QString)));
    connect(signalAdapter, SIGNAL(connection_failure(QString)),
            this, SLOT(unknown_db_error(QString)));
    connect(signalAdapter, SIGNAL(server_rejected_connection(QString)),
            this, SLOT(unknown_db_error(QString)));
    connect(signalAdapter, SIGNAL(transaction_resolution_unknown(QString)),
            this, SLOT(unknown_db_error(QString)));
    connect(signalAdapter, SIGNAL(protocol_violation(QString)),
            this, SLOT(unknown_db_error(QString)));
    connect(signalAdapter, SIGNAL(database_error(QString)),
            this, SLOT(unknown_db_error(QString)));

    // Signals that we will handle specially:
    connect(signalAdapter, SIGNAL(unable_to_establish_connection(QString)), this, SLOT(db_connection_failed(QString)));

    // Setup the notification pump
    notificationTimer = new QTimer(this);
    notificationTimer->setInterval(1000);
    connect(notificationTimer, SIGNAL(timeout()),
            this, SLOT(notification_pump()));

    db_connect(databaseName, hostname, port, username, password, station);
}

DatabaseLiveDataSource::~DatabaseLiveDataSource() {
    notificationTimer->stop();
    wdb_disconnect();
}

void DatabaseLiveDataSource::unknown_db_error(QString message) {
    emit database_error(message);
}

void DatabaseLiveDataSource::db_connection_failed(QString message) {
    notificationTimer->stop();
    emit connection_failed(message);
}

void DatabaseLiveDataSource::db_connect(
        QString dbName,
        QString dbHostname,
        int port,
        QString username,
        QString password,
        QString station) {

    QString dbPort = QString::number(port);

    QString target = dbName;
    if (!dbHostname.isEmpty()) {
        target += "@" + dbHostname;

        if (!dbPort.isEmpty())
            target += ":" + dbPort;
    }

    qDebug() << "Connecting to target" << target << "as user" << username;

    if (!wdb_connect(target.toAscii().constData(),
                     username.toAscii().constData(),
                     password.toAscii().constData(),
                     station.toAscii().constData())) {
        // Failed to connect.
        connected = false;
    } else {
        notificationTimer->start();
        connected = true;
    }
}

AbstractLiveData* DatabaseLiveDataSource::getLiveData() {
    live_data_record rec;

    rec = wdb_get_live_data();

    DatabaseLiveData* dld = new DatabaseLiveData(rec);

    return dld;
}

void DatabaseLiveDataSource::notification_pump() {
    if (wdb_live_data_available()) {
        qDebug() << "Live data available";
        emit liveDataRefreshed();
    }
}
