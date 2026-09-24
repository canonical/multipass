include("${CMAKE_CURRENT_LIST_DIR}/../vcpkg/triplets/community/s390x-linux-release.cmake")

include("${CMAKE_CURRENT_LIST_DIR}/common.cmake")

# Workaround for dbus and qtbase: when vcpkg sets VCPKG_CMAKE_SYSTEM_NAME, CMake may treat
# native s390x builds as cross-compiling.
set(VCPKG_CMAKE_CONFIGURE_OPTIONS "-DCMAKE_CROSSCOMPILING:BOOL=OFF")

# Disable Valgrind which glib pulls in and compiles. Valgrind will not compile on s390x
# with newer versions of clang.
if(PORT STREQUAL "glib")
    set(VCPKG_C_FLAGS "-DNVALGRIND")
    set(VCPKG_CXX_FLAGS "-DNVALGRIND")
endif()
