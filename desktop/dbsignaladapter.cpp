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

#include "dbsignaladapter.h"

#include <QtDebug>

#define CHK_SQLSTATE(code) (sqlstate[0] == code[0] && sqlstate[1] == code[1])
#define CHK_CODE(code) (sqlstate[2] == code[0] && sqlstate[3] == code[1] && sqlstate[4] == code[2])

DBSignalAdapter::DBSignalAdapter(QObject *parent) :
    QObject(parent)
{
}

void DBSignalAdapter::raiseDatabaseError(long sqlcode,
                                         int sqlerrml,
                                         QString sqlerrmc,
                                         long sqlerrd[6],
                                         char sqlwarn[8],
                                         char sqlstate[5]) {

    qDebug() << "sqlcode:" << sqlcode;
    qDebug() << "sqlerrm.sqlerrml:" << sqlerrml;
    qDebug() << "sqlerrm.sqlerrmc:" << sqlerrmc;
    qDebug() << "sqlerrd:"
             << sqlerrd[0] << sqlerrd[1] << sqlerrd[2] << sqlerrd[3]
             << sqlerrd[4] << sqlerrd[5];
    qDebug() << "sqlwarn:"
             << sqlwarn[0] << sqlwarn[1] << sqlwarn[2] << sqlwarn[3]
             << sqlwarn[4] << sqlwarn[5] << sqlwarn[6] << sqlwarn[7];
    qDebug() << "sqlstate:"
             << sqlstate[0] << sqlstate[1] << sqlstate[2] << sqlstate[3]
             << sqlstate[4];

    // sqlstate contains more information on the error. Lets decode it.
    if (CHK_SQLSTATE("00")) {
        // Successful Completion
    } else if (CHK_SQLSTATE("01")) {
        // Warning
        emit database_warning(sqlerrmc);
    } else if (CHK_SQLSTATE("02")) {
        // No Data
    } else if (CHK_SQLSTATE("03")) {
        // Statement Not Yet Complete
    } else if (CHK_SQLSTATE("08")) {
        // Connection Exception
        if (CHK_CODE("000")) {
            // Connection Exception
            emit connection_exception(sqlerrmc);
        } else if (CHK_CODE("003")) {
            // Connection does not exist
            emit connection_does_not_exist(sqlerrmc);
        } else if (CHK_CODE("006")) {
            // Connection Failure
            emit connection_exception(sqlerrmc);
        } else if (CHK_CODE("001")) {
            // SQL Client unable to establish sql connection
            emit unable_to_establish_connection(sqlerrmc);
        } else if (CHK_CODE("004")) {
            // sqlserver rejected establishment of sqlconnection
            emit server_rejected_connection(sqlerrmc);
        } else if (CHK_CODE("007")) {
            // transaction resolution unknown
            emit transaction_resolution_unknown(sqlerrmc);
        } else if (CHK_CODE("P01")) {
            // Protocol violation
            emit protocol_violation(sqlerrmc);
        }
    } else {
        // These error classes are not handled specially at the moment.
        emit database_error(sqlerrmc);

        if (CHK_SQLSTATE("09")) {
            // Triggered Action Exception
        } else if (CHK_SQLSTATE("0A")) {
            // Feature not supported
        } else if (CHK_SQLSTATE("0B")) {
            // Invalid Transaction Initiation
        } else if (CHK_SQLSTATE("0F")) {
            // Locator Exception
        } else if (CHK_SQLSTATE("0L")) {
            // Invalid Grantor
        } else if (CHK_SQLSTATE("0P")) {
            // Invalid Role Specification
        } else if (CHK_SQLSTATE("20")) {
            // Case not found
        } else if (CHK_SQLSTATE("21")) {
            // Cardinality violation
        } else if (CHK_SQLSTATE("22")) {
            // Data Exception
        } else if (CHK_SQLSTATE("23")) {
            // Integrity Constraint Violation
        } else if (CHK_SQLSTATE("24")) {
            // Invalid Cursor State
        } else if (CHK_SQLSTATE("25")) {
            // Invalid Transaction State
        } else if (CHK_SQLSTATE("26")) {
            // Invalid SQL Statement Name
        } else if (CHK_SQLSTATE("27")) {
            // Triggered Data Change Violation
        } else if (CHK_SQLSTATE("28")) {
            // Invalid Authorization Specification
        } else if (CHK_SQLSTATE("2B")) {
            // Dependent Privilege Descriptors Still Exist
        } else if (CHK_SQLSTATE("0D")) {
            // Invalid Transaction Termination
        } else if (CHK_SQLSTATE("2F")) {
            // SQL Routine Exception
        } else if (CHK_SQLSTATE("34")) {
            // Invalid Cursor Name
        } else if (CHK_SQLSTATE("38")) {
            // External Routine Exception
        } else if (CHK_SQLSTATE("39")) {
            // External Routine Invocation Exception
        } else if (CHK_SQLSTATE("3B")) {
            // Savepoint Exception
        } else if (CHK_SQLSTATE("3D")) {
            // Invalid Catalog Name
        } else if (CHK_SQLSTATE("3F")) {
            // Invalid Schema Name
        } else if (CHK_SQLSTATE("40")) {
            // Transaction Rollback
        } else if (CHK_SQLSTATE("42")) {
            // Syntax Error or Access Rule Violation
        } else if (CHK_SQLSTATE("44")) {
            // With check option violation
        } else if (CHK_SQLSTATE("53")) {
            // Insufficient Resources
        } else if (CHK_SQLSTATE("54")) {
            // Program Limit Exceeded
        } else if (CHK_SQLSTATE("55")) {
            // Object not in prerequisite State
        } else if (CHK_SQLSTATE("57")) {
            // Operator intervention
        } else if (CHK_SQLSTATE("58")) {
            // System error
        } else if (CHK_SQLSTATE("F0")) {
            // Configuration file error
        } else if (CHK_SQLSTATE("HV")) {
            // Foreign Data Wrapper Error (SQL/MED)
        } else if (CHK_SQLSTATE("P0")) {
            // PL/pgSQL Error
        } else if (CHK_SQLSTATE("XX")) {
            // Internal Error
        } else {
            qDebug() << "Unknown SQL State class";

        }
    }
}
