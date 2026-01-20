set(VCPKG_TARGET_ARCHITECTURE s390x)
set(VCPKG_CRT_LINKAGE dynamic)
set(VCPKG_LIBRARY_LINKAGE static)
set(VCPKG_CMAKE_SYSTEM_NAME Linux)
set(VCPKG_BUILD_TYPE release)

# Workaround for dbus and qtbase: when vcpkg sets VCPKG_CMAKE_SYSTEM_NAME, CMake may treat
# native s390x builds as cross-compiling.
set(VCPKG_CMAKE_CONFIGURE_OPTIONS "-DCMAKE_CROSSCOMPILING:BOOL=OFF")
