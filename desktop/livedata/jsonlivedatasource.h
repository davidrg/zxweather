/*****************************************************************************
 *            Created: 23/09/2012
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

#ifndef JSONDATASOURCE_H
#define JSONDATASOURCE_H

#include "livedatasource.h"
#include <QScopedPointer>
#include <QTimer>
#include <QNetworkAccessManager>

class QNetworkReply;

class JsonLiveDataSource : public AbstractLiveDataSource
{
    Q_OBJECT
public:
    explicit JsonLiveDataSource(QString url, QObject *parent = 0);
    
    AbstractLiveData* getLiveData();

    /* The default is to return true (this datasource doesn't really maintain
     * a connection to anything).
    bool isConnected();
     */
signals:
    void networkError(QString errorMessage);

private slots:
    void liveDataPoll();
    void dataReady(QNetworkReply* reply);
    
private:
    /** Checks to see if the data is out of date.
     * @return True if the data is fine, false if it is out of date.
     */
    bool CheckDataAge();

    QScopedPointer<QTimer> notificationTimer;
    QString url;
    QString json_data;
    QScopedPointer<QNetworkAccessManager> netAccessManager;
};

#endif // JSONDATASOURCE_H
