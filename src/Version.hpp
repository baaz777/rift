#pragma once

/**
 * @brief Engine version in major.minor.patch.release order.
 * @author Alex (<https://github.com/lextpf>)
 * @ingroup Core
 *
 * cmake parses these four defines. Keep each on one line in the existing form.
 */

#define RIFT_VERSION_MAJOR 0

#define RIFT_VERSION_MINOR 2

#define RIFT_VERSION_PATCH 0

#define RIFT_VERSION_RELEASE 0

/// two levels expand version macros before stringification.
#define RIFT_VERSION_STRINGIFY_(major, minor, patch, release) \
    #major "." #minor "." #patch "." #release
#define RIFT_VERSION_STRINGIFY(major, minor, patch, release) \
    RIFT_VERSION_STRINGIFY_(major, minor, patch, release)

#define RIFT_VERSION        \
    RIFT_VERSION_STRINGIFY( \
        RIFT_VERSION_MAJOR, RIFT_VERSION_MINOR, RIFT_VERSION_PATCH, RIFT_VERSION_RELEASE)
