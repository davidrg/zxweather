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

#ifndef DATABASEDATASOURCE_H
#define DATABASEDATASOURCE_H

#include "datasource.h"
#include "dbsignaladapter.h"

class QTimer;

class DatabaseDataSource : public AbstractDataSource
{
    Q_OBJECT
public:
    explicit DatabaseDataSource(
            QString databaseName,
            QString hostname,
            int port,
            QString username,
            QString password,
            QString station,
            QObject *parent = 0);

    ~DatabaseDataSource();

    AbstractLiveData* getLiveData();

    bool isConnected() {return connected;}
    
signals:
    void connection_failed(QString);
    void database_error(QString message);

private slots:
    /**
     * @brief Called when connecting to the database fails.
     *
     * It displays a popup message from the system tray icon containing details
     * of the problem.
     */
    void db_connection_failed(QString);

    /**
     * @brief Called whenever a database error occurs that is not a connection
     * failure.
     *
     * @param message The error message from the database layer.
     */
    void unknown_db_error(QString message);

    /** Polls for notification messages from the database layer.
     */
    void notification_pump();

private:
    bool db_connect(QString databaseName,
                 QString hostname,
                 int port,
                 QString username,
                 QString password, QString station);

    bool connected;
    DBSignalAdapter *signalAdapter;
    QTimer *notificationTimer;
};

#endif // DATABASEDATASOURCE_H
