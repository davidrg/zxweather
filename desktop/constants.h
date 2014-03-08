#ifndef CONSTANTS_H
#define CONSTANTS_H

#include <QByteArray>

namespace Constants {

    const QByteArray USER_AGENT = "zxweather-desktop/1.0";
    const QByteArray VERSION_STR = "1.0.0";

    /** Document number for the zxweather installation manual for *this*
     * release of zxweather.
     */
    const QString INSTALLATION_REFERENCE_MANUAL = "DAZW-IG03";

    /** The version of zxweather that first included this particular version
     * of the desktop client.
     */
    const QString ZXWEATHER_VERSION = VERSION_STR;
}
#endif // CONSTANTS_H
