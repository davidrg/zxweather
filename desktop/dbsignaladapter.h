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


#ifndef DBSIGNALADAPTER_H
#define DBSIGNALADAPTER_H

#ifndef NO_ECPG


#include <QObject>
#include <Qtimer>
#include "database.h"

/** The DBSignalAdapter class converts database errors and other events
 * into signals that can be consumed by Qt classes. It also handles polling
 * the database for current conditions and new samples.
 */
class DBSignalAdapter : public QObject
{
    Q_OBJECT

public:

    static void connectInstance(QString host, QString username, QString password, QString stationCode) ;
    static DBSignalAdapter& getInstance();

    /**
     * @brief raiseDatabaseError is called by the database layer when an error
     * occurs. This causes the suitable signals to be raised for handling by
     * other components in the system.
     *
     * The various parameters can be pulled out of the sqlca global struct.
     *
     * @param sqlcode Old-style SQL State. Negative is an error.
     * @param sqlerrml Length of the error message.
     * @param sqlerrmc The error message.
     * @param sqlerrd Basic information about the error
     * @param sqlwarn Basic information about the warning
     * @param sqlstate Error/warning code.
     */
    void raiseDatabaseError(long sqlcode,
                            int sqlerrml,
                            QString sqlerrmc,
                            long sqlerrd[6],
                            char sqlwarn[8],
                            char sqlstate[5]);

signals:

    /**
     * @brief _error is emitted for all errors.
     * @param message Error message from the database layer.
     */
    void error(QString message);

    void liveDataUpdated(live_data_record rec);
    void newImage(int imageId);
    void newSample(int sampleId);

private slots:
    void notificationPump(bool force = false);

private:
    DBSignalAdapter();
    ~DBSignalAdapter() {}

    bool isConnected;
    QString username;
    QString password;
    QString hostname;
    QString stationCode;

    QTimer notificationTimer;

    DBSignalAdapter(DBSignalAdapter const&); /* Not implemented. Don't use it. */
    void operator=(DBSignalAdapter const&); /* Not implemented. Don't use it. */
};

#endif

#endif // DBSIGNALADAPTER_H
