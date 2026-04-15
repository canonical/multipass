vcpkg_from_github(
    OUT_SOURCE_PATH SOURCE_PATH
    REPO tukaani-project/xz-embedded
    REF v2024-12-30
    SHA512 f02bef41789bf0a48079f5e7d3309d5c8d2aa526604114092828f92ee338f16dc8de3e18892587c2611eca1df846964f4983f9901fcf36ab94f4a3ac7bb52c94
    HEAD_REF master
)

file(COPY "${CMAKE_CURRENT_LIST_DIR}/CMakeLists.txt" DESTINATION "${SOURCE_PATH}")

vcpkg_cmake_configure(SOURCE_PATH "${SOURCE_PATH}")
vcpkg_cmake_install()
vcpkg_cmake_config_fixup(PACKAGE_NAME xz-embedded CONFIG_PATH lib/cmake/xz-embedded)

file(REMOVE_RECURSE "${CURRENT_PACKAGES_DIR}/debug/include")
vcpkg_install_copyright(FILE_LIST "${SOURCE_PATH}/COPYING")