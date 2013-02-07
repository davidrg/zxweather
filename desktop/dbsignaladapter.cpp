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

    emit error(sqlerrmc);
}
