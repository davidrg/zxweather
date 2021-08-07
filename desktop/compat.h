#ifndef COMPAT_H
#define COMPAT_H

/*
 * Some handy macros for easing portability between Qt versions.
 */


/*
 * toTime_t() is deprecated in most of 5.x and removed entirely in 6.0
 * Its replacement, to/fromSecsSinceEpoch(), doesn't appear until Qt 5.8.
 *
 * >>> Can be removed once Minimum is >= Qt 5.8
 */
#if (QT_VERSION >= QT_VERSION_CHECK(5,8,0))
#define TO_UNIX_TIME(dt) (dt.toSecsSinceEpoch())
#define FROM_UNIX_TIME(ts) (QDateTime::fromSecsSinceEpoch(ts))
#else
#define TO_UNIX_TIME(dt) (dt.toTime_t())
#define FROM_UNIX_TIME(ts) (QDateTime::fromTime_t(ts))
#endif

#endif // COMPAT_H
