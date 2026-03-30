vcpkg_from_github(
    OUT_SOURCE_PATH SOURCE_PATH
    REPO atilaneves/premock
    REF ef6e62da55760b6d81f053b34cde82b883eb27b8
    SHA512 ae67a2eee725d79b9d548fa7470fd7518430db66d823a5838c5b24a0729c0ab325e9699b6ec7074ee11f73dedc6fec9243a70d2d5659caee825e902b5b5a2dc4
    HEAD_REF master
)

file(INSTALL "${SOURCE_PATH}/premock.hpp"
     DESTINATION "${CURRENT_PACKAGES_DIR}/include"
)

file(INSTALL "${CMAKE_CURRENT_LIST_DIR}/premock-config.cmake"
     DESTINATION "${CURRENT_PACKAGES_DIR}/share/premock"
)

vcpkg_install_copyright(FILE_LIST "${SOURCE_PATH}/LICENSE")