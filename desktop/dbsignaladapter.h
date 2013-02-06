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
 * into signals that can be consumed by Qt classes.
 */
class DBSignalAdapter : public QObject
{
    Q_OBJECT
public:
    /**
     * @brief DBSignalAdapter creates a new DB Signal Adapter.
     * @param parent Who owns the signal adapter.
     */
    explicit DBSignalAdapter(QObject *parent = 0);
    
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

    /**
     * @brief database_error is emitted when ever a database error is occurred
     * that doesn't cause one of the other signals to be raised.
     * @param message Error message from the database layer.
     */
    void database_error(QString message);

    /**
     * @brief database_warning is emitted when ever a warning occurs in the
     * database layer.
     * @param message Warning message from the database layer.
     */
    void database_warning(QString message);

    // Connection Exceptions

    void connection_exception(QString message);

    /**
     * @brief connection_does_not_exist is emitted when a query or some other
     * database operation is performed with no open connection.
     * @param message Message from lower down.
     */
    void connection_does_not_exist(QString message);
    void connection_failure(QString message);

    /**
     * @brief unable_to_establish_connection is emitted when connecting to the
     * server failed for some reason.
     * @param message Error message from lower down.
     */
    void unable_to_establish_connection(QString message);

    /**
     * @brief server_rejected_connection is emitted if the server rejected the
     * connection for some reason. The message probably contains more details.
     * @param message Message from lower down.
     */
    void server_rejected_connection(QString message);
    void transaction_resolution_unknown(QString message);
    void protocol_violation(QString message);
};

#endif // DBSIGNALADAPTER_H
