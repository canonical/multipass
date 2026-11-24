set(VCPKG_TARGET_ARCHITECTURE x64)
set(VCPKG_CRT_LINKAGE dynamic)
set(VCPKG_LIBRARY_LINKAGE static)

# The shared MinGW closure lives outside the port directories, so add it to the
# ABI hash explicitly; otherwise lockfile bumps won't invalidate cached binaries.
if(PORT MATCHES "^(qemu|ubuntu-insights)$")
    list(APPEND VCPKG_HASH_ADDITIONAL_FILES
        "${CMAKE_CURRENT_LIST_DIR}/../vcpkg-port-utils/mingw/mingw64-toolchain.lock"
        "${CMAKE_CURRENT_LIST_DIR}/../vcpkg-port-utils/mingw/get-mingw-packages.cmake"
        "${CMAKE_CURRENT_LIST_DIR}/../vcpkg-port-utils/mingw/acquire-mingw-msys.cmake"
    )
endif()
