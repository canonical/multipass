vcpkg_from_github(
    OUT_SOURCE_PATH SOURCE_PATH
    REPO pocoproject/poco
    REF "poco-1.13.3-release"
    SHA512 084064fb462c9e7993d069ebdf395802af900ed92c5b294465a2c246162bb86caa3505985de329e8110d3e9fb3bc39ae9536d523843729d4ed5ce00c35289d92
    HEAD_REF devel
    PATCHES
        # Fix embedded copy of pcre in static linking mode
        0001-static-pcre.patch
        # Add the support of arm64-windows
        0002-arm64-pcre.patch
        0003-fix-dependency.patch
        0004-fix-feature-sqlite3.patch
        0005-fix-error-c3861.patch
        0007-find-pcre2.patch
        0008-fix-zip-cmake-dependencies.patch
)

file(REMOVE "${SOURCE_PATH}/Foundation/src/pcre2.h")
file(REMOVE "${SOURCE_PATH}/cmake/V39/FindEXPAT.cmake")
file(REMOVE "${SOURCE_PATH}/cmake/V313/FindSQLite3.cmake")
# vcpkg's PCRE2 does not provide a FindPCRE2, and the bundled one seems to work fine
# file(REMOVE "${SOURCE_PATH}/cmake/FindPCRE2.cmake")
file(REMOVE "${SOURCE_PATH}/XML/src/expat_config.h")
file(REMOVE "${SOURCE_PATH}/cmake/FindMySQL.cmake")

# define Poco linkage type
string(COMPARE EQUAL "${VCPKG_CRT_LINKAGE}" "static" POCO_MT)

vcpkg_check_features(OUT_FEATURE_OPTIONS FEATURE_OPTIONS
    FEATURES
        crypto      ENABLE_CRYPTO
        netssl      ENABLE_NETSSL
        pdf         ENABLE_PDF
        sqlite3     ENABLE_DATA_SQLITE
        postgresql  ENABLE_DATA_POSTGRESQL
)

# POCO_ENABLE_NETSSL_WIN: 
# Use the unreleased NetSSL_Win module instead of (OpenSSL) NetSSL.
# This is a variable which can be set in the triplet file.
if(POCO_ENABLE_NETSSL_WIN)
    string(REPLACE "ENABLE_NETSSL" "ENABLE_NETSSL_WIN" FEATURE_OPTIONS "${FEATURE_OPTIONS}")
    list(APPEND FEATURE_OPTIONS "-DENABLE_NETSSL:BOOL=OFF")
endif()

if ("mysql" IN_LIST FEATURES OR "mariadb" IN_LIST FEATURES)
    set(POCO_USE_MYSQL ON)
else()
    set(POCO_USE_MYSQL OFF)
endif()

vcpkg_cmake_configure(
    SOURCE_PATH "${SOURCE_PATH}"
    OPTIONS
        ${FEATURE_OPTIONS}
        # force to use dependencies as external
        -DPOCO_UNBUNDLED=ON
        # Define linking feature
        -DPOCO_MT=${POCO_MT}
        -DENABLE_TESTS=OFF
        # Allow enabling and disabling components
        -DENABLE_NETSSL=OFF
        -DENABLE_CRYPTO=OFF
        -DENABLE_JWT=OFF
        -DENABLE_ENCODINGS=OFF
        -DENABLE_XML=OFF
        -DENABLE_JSON=OFF
        -DENABLE_MONGODB=OFF
        -DENABLE_DATA_SQLITE=OFF
        -DENABLE_REDIS=OFF
        -DENABLE_UTIL=OFF
        -DENABLE_NET=OFF
        -DENABLE_ZIP=ON
        -DENABLE_PAGECOMPILER=OFF
        -DENABLE_PAGECOMPILER_FILE2PAGE=OFF
        -DENABLE_ACTIVERECORD_COMPILER=OFF
        -DENABLE_ACTIVERECORD=OFF
        -DENABLE_DATA=OFF
        -DENABLE_PROMETHEUS=OFF
        -DENABLE_POCODOC=ON # this is needed for vcpkg_cmake_install to install the some necessary cmake configuration files
        -DENABLE_DATA_MYSQL=${POCO_USE_MYSQL}
    MAYBE_UNUSED_VARIABLES # these are only used when if(MSVC)
        POCO_DISABLE_INTERNAL_OPENSSL
        POCO_MT
)

vcpkg_cmake_install()

vcpkg_copy_pdbs()

if(VCPKG_LIBRARY_LINKAGE STREQUAL "static")
    file(REMOVE_RECURSE "${CURRENT_PACKAGES_DIR}/bin" "${CURRENT_PACKAGES_DIR}/debug/bin")
endif()

# Copy additional include files not part of any libraries
if(EXISTS "${CURRENT_PACKAGES_DIR}/include/Poco/SQL")
    file(COPY "${SOURCE_PATH}/Data/include" DESTINATION "${CURRENT_PACKAGES_DIR}")
endif()
if(EXISTS "${CURRENT_PACKAGES_DIR}/include/Poco/SQL/MySQL")
    file(COPY "${SOURCE_PATH}/Data/MySQL/include" DESTINATION "${CURRENT_PACKAGES_DIR}")
endif()
if(EXISTS "${CURRENT_PACKAGES_DIR}/include/Poco/SQL/ODBC")
    file(COPY "${SOURCE_PATH}/Data/ODBC/include" DESTINATION "${CURRENT_PACKAGES_DIR}")
endif()
if(EXISTS "${CURRENT_PACKAGES_DIR}/include/Poco/SQL/PostgreSQL")
    file(COPY "${SOURCE_PATH}/Data/PostgreSQL/include" DESTINATION "${CURRENT_PACKAGES_DIR}")
    file(REMOVE_RECURSE "${CURRENT_PACKAGES_DIR}/include/libpq")
endif()
if(EXISTS "${CURRENT_PACKAGES_DIR}/include/Poco/SQL/SQLite")
    file(COPY "${SOURCE_PATH}/Data/SQLite/include" DESTINATION "${CURRENT_PACKAGES_DIR}")
endif()

if(VCPKG_TARGET_IS_WINDOWS)
  vcpkg_cmake_config_fixup(CONFIG_PATH cmake)
else()
  vcpkg_cmake_config_fixup(CONFIG_PATH lib/cmake/Poco)
endif()

file(REMOVE_RECURSE "${CURRENT_PACKAGES_DIR}/debug/include")
file(REMOVE_RECURSE "${CURRENT_PACKAGES_DIR}/debug/share")

file(COPY "${CMAKE_CURRENT_LIST_DIR}/usage" DESTINATION "${CURRENT_PACKAGES_DIR}/share/${PORT}")
file(INSTALL "${SOURCE_PATH}/LICENSE" DESTINATION "${CURRENT_PACKAGES_DIR}/share/${PORT}" RENAME copyright)

vcpkg_install_copyright(FILE_LIST "${SOURCE_PATH}/LICENSE")
