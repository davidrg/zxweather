#ifndef CONSTANTS_H
#define CONSTANTS_H

#include <QByteArray>

#include "version.h"

namespace Constants {

    const QByteArray USER_AGENT = "zxweather-desktop/" APP_VERSION_MAJ_MIN;
    const QByteArray VERSION_STR = APP_VERSION_STR;

    /** Document number for the zxweather installation manual for *this*
     * release of zxweather.
     */
    const QString INSTALLATION_REFERENCE_MANUAL = "DAZW-IG03";

    /** The version of zxweather that first included this particular version
     * of the desktop client.
     */
    const QString ZXWEATHER_VERSION = APP_VERSION_STR;
}
#endif // CONSTANTS_H
