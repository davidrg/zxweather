#ifndef CONFIGWIZARD_PRIVATE_H
#define CONFIGWIZARD_PRIVATE_H

#include "configwizard.h"

const QString DB_NAME = "CONTEST";

const QString LOGO_PIXMAP = ":/images/config_wizard/logo";

const QString WATERMARK_PIXMAP = ":/images/config_wizard/watermark";

/*   Field: isLocalAccessType
 *    Type: bool
 *    Page: AccessTypePage
 * Purpose: If we are using local access or not.
 */
#define LOCAL_ACCESS_TYPE_FIELD "isLocalAccessType"

/*   Field: database
 *    Type: QString
 *    Page: DatabaseDetailsPage
 * Purpose: Database name
 */
#define DATABASE_FIELD "database"

/*   Field: hostname
 *    Type: QString
 *    Page: DatabaseDetailsPage
 * Purpose: Database server hostname
 */
#define DATABASE_HOSTNAME_FIELD "hostname"

/*   Field: port
 *    Type: int
 *    Page: DatabaseDetailsPage
 * Purpose: Database server port
 */
#define DATABASE_PORT_FIELD "port"

/*   Field: username
 *    Type: QString
 *    Page: DatabaseDetailsPage
 * Purpose: Database username
 */
#define DATABASE_USERNAME_FIELD "username"

/*   Field: password
 *    Type: QString
 *    Page: DatabaseDetailsPage
 * Purpose: Database password
 */
#define DATABASE_PASSWORD_FIELD "password"

/*   Field: multiple_stations
 *    Type: bool
 *    Page: AccessTypePage
 * Purpose: True if there is more than one weather station available
 */
#define MULTIPLE_STATIONS_AVAILABLE_FIELD "multiple_stations"

/*   Field: station_list
 *    Type: List<DbUtil::StationInfo>
 *    Page: AccessTypePage
 * Purpose: A list of all available stations
 */
#define STATION_LIST_FIELD "station_list"

/*   Field: station
 *    Type: DbUtil::StationInfo
 *    Page: AccessTypePage
 * Purpose: The first station available. This is used as the chosen station
 *          when only one station is available
 */
#define FIRST_STATION_FIELD "station"

/*   Field: stationCode
 *    Type: QString
 *    Page: SelectStationPage
 * Purpose: The code of the chosen station
 */
#define SELECTED_STATION_CODE "stationCode"

/*   Field: stationTitle
 *    Type: QString
 *    Page: SelectStationPage
 * Purpose: The title of the chosen station
 */
#define SELECTED_STATION_TITLE "stationTitle"

/*   Field: webBaseUrl *
 *    Type: QString
 *    Page: InternetSiteInfoPage
 * Purpose: The base URL for the web interface
 */
#define BASE_URL_FIELD "webBaseUrl"

/*   Field: serverStationAvailability
 *    Type: List<ServerStation>
 *    Page: InternetSiteInfoPage
 * Purpose: List of all stations found and their status on any known zxweather server.
 */
#define SERVER_STATION_AVAILABILITY "serverStationAvailability"

/*   Field: serverAvailable
 *    Type: bool
 *    Page: AccessTypePage
 * Purpose: Stores if there is an internet weather server available
 */
#define SERVER_AVAILABLE "inetServerAvailable"

/*   Field: serverHostname
 *    Type: QString
 *    Page: AccessTypePage
 * Purpose: Stores hostname of internet weather server
 */
#define SERVER_HOSTNAME "inetServerHostname"

/*   Field: serverPort
 *    Type: int
 *    Page: AccessTypePage
 * Purpose: Stores port for internet weather server
 */
#define SERVER_PORT "inetServerPort"

/*   Field:
 *    Type:
 *    Page:
 * Purpose:
 */

#endif // CONFIGWIZARD_PRIVATE_H
