#ifndef CONSTANTS_H
#define CONSTANTS_H

#include <QByteArray>
#include <QString>

#include "version.h"

#if (QT_VERSION < QT_VERSION_CHECK(5,0,0))
// Otherwise we get garbage...
#define DEGREE_SYMBOL "\xB0"
#define SQUARED_SYMBOL "\xB2"
#else
#define SQUARED_SYMBOL "\u00B2"
#define DEGREE_SYMBOL "\u00B0"
#endif

#define TEMPERATURE_SYMBOL DEGREE_SYMBOL "C"
#define IMPERIAL_TEMPERATURE_SYMBOL DEGREE_SYMBOL "F"

//#define TEMPERATURE_SYMBOL "\u2103"
//#define IMPERIAL_TEMPERATURE_SYMBOL "\u2109"

namespace Constants {

    const QByteArray USER_AGENT = "zxweather-desktop/" APP_VERSION_MAJ_MIN;
    const QByteArray VERSION_STR = APP_VERSION_STR;

    /** Document number for the zxweather installation manual for *this*
     * release of zxweather.
     */
    const QString INSTALLATION_REFERENCE_MANUAL = "DAZW-IG03";

    const QString APP_NAME = "zxweather-desktop";

    /** The version of zxweather that first included this particular version
     * of the desktop client.
     */
    const QString ZXWEATHER_VERSION = APP_VERSION_STR;

#ifdef SINGLE_INSTANCE
    /** Prefix for the single instance lock name
     *
     */
    const QString SINGLE_INSTANCE_LOCK_PREFIX = "zxw-desktop-";
#endif

    // Size for image thumbnails.
    const int THUMBNAIL_WIDTH = 304;
    const int THUMBNAIL_HEIGHT = 171;

    // Size for mini image thumbnails
    const int MINI_THUMBNAIL_WIDTH = 90;
    const int MINI_THUMBNAIL_HEIGHT = 51;
}
#endif // CONSTANTS_H
