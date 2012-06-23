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

#include <QObject>

/** The DBSignalAdapter class converts database errors and other events
 * into signals.
 */
class DBSignalAdapter : public QObject
{
    Q_OBJECT
public:
    explicit DBSignalAdapter(QObject *parent = 0);
    
    void raiseDatabaseError(long sqlcode,
                            int sqlerrml,
                            QString sqlerrmc,
                            long sqlerrd[6],
                            char sqlwarn[8],
                            char sqlstate[5]);

signals:
    void database_error(QString message);

    // Connection Exceptions
    void connection_exception(QString message);
    void connection_does_not_exist(QString message);
    void connection_failure(QString message);
    void unable_to_establish_connection(QString message);
    void server_rejected_connection(QString message);
    void transaction_resolution_unknown(QString message);
    void protocol_violation(QString message);
};

#endif // DBSIGNALADAPTER_H
