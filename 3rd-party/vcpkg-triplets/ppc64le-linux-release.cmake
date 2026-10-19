set(VCPKG_TARGET_ARCHITECTURE ppc64le)
set(VCPKG_CRT_LINKAGE dynamic)
set(VCPKG_LIBRARY_LINKAGE static)
set(VCPKG_CMAKE_SYSTEM_NAME Linux)
set(VCPKG_BUILD_TYPE release)

# Workaround for dbus and qtbase: when vcpkg sets VCPKG_CMAKE_SYSTEM_NAME, CMake may treat
# native ppc64le builds as cross-compiling.
set(VCPKG_CMAKE_CONFIGURE_OPTIONS "-DCMAKE_CROSSCOMPILING:BOOL=OFF")

# Workaround for GCC 13 internal compiler errors on ppc64le
# Use Clang which is more stable on this architecture
set(VCPKG_CMAKE_CONFIGURE_OPTIONS "${VCPKG_CMAKE_CONFIGURE_OPTIONS};-DCMAKE_C_COMPILER=clang;-DCMAKE_CXX_COMPILER=clang++")
