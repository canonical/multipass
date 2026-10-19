vcpkg_download_distfile(distfile
    URLS https://gitlab.com/libssh/libssh-mirror/-/archive/libssh-${VERSION}/libssh-mirror-libssh-${VERSION}.tar.gz
    FILENAME libssh-${VERSION}.tar.gz
    SHA512 950975f9b936cde668be0e09c2fa01cb5f413e912e124eac1fbbddf53da5417b330cb450a62d5cec6cfa0e53e1414e03c762f1c2c96d17b716444478104ff54f
)
vcpkg_extract_source_archive(SOURCE_PATH
    ARCHIVE "${distfile}"
    PATCHES
        0001-export-pkgconfig-file.patch
        0003-no-source-write.patch
        0004-file-permissions-constants.patch
        android-glob-tilde.diff
        multipass-patches/0001-connector-stderr.patch
        multipass-patches/0002-sftpserver-increase-handle-count.patch
        multipass-patches/0003-fix-handle-realloc-failure.patch
)

vcpkg_check_features(OUT_FEATURE_OPTIONS FEATURE_OPTIONS
    FEATURES
        pcap    WITH_PCAP
        server  WITH_SERVER
        zlib    WITH_ZLIB
)

vcpkg_cmake_configure(
    SOURCE_PATH "${SOURCE_PATH}"
    OPTIONS
        ${FEATURE_OPTIONS}
        -DCMAKE_REQUIRE_FIND_PACKAGE_OpenSSL=ON
        -DWITH_EXAMPLES=OFF
        -DWITH_GSSAPI=OFF
        -DWITH_NACL=OFF
        -DWITH_SYMBOL_VERSIONING=OFF
)

vcpkg_cmake_install()
vcpkg_copy_pdbs()
vcpkg_fixup_pkgconfig()

if(VCPKG_LIBRARY_LINKAGE STREQUAL "static")
    vcpkg_replace_string("${CURRENT_PACKAGES_DIR}/include/libssh/libssh.h"
        "#ifdef LIBSSH_STATIC"
        "#if 1"
    )
endif()

vcpkg_cmake_config_fixup(CONFIG_PATH lib/cmake/libssh)

file(READ "${CURRENT_PACKAGES_DIR}/share/libssh/libssh-config.cmake" cmake_config)
file(WRITE "${CURRENT_PACKAGES_DIR}/share/libssh/libssh-config.cmake" "
include(CMakeFindDependencyMacro)
if(MINGW32)
    set(THREADS_PREFER_PTHREAD_FLAG ON)
    find_dependency(Threads)
endif()
find_dependency(OpenSSL)
if(\"${WITH_ZLIB}\")
    find_dependency(ZLIB)
endif()
${cmake_config}"
)

file(REMOVE_RECURSE "${CURRENT_PACKAGES_DIR}/debug/include")

vcpkg_install_copyright(FILE_LIST "${SOURCE_PATH}/COPYING")
