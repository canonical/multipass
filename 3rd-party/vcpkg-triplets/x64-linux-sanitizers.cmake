set(VCPKG_TARGET_ARCHITECTURE x64)
set(VCPKG_CRT_LINKAGE dynamic)
set(VCPKG_LIBRARY_LINKAGE static)

set(VCPKG_CMAKE_SYSTEM_NAME Linux)

# Force debug build
# Force compilers
set(CMAKE_C_COMPILER clang)
set(CMAKE_CXX_COMPILER clang++)

set(VCPKG_C_FLAGS   "${VCPKG_C_FLAGS} -fsanitize=address,undefined,leak -fno-omit-frame-pointer")
set(VCPKG_CXX_FLAGS "${VCPKG_CXX_FLAGS} -fsanitize=address,undefined,leak -fno-omit-frame-pointer")

# Qt builds host tools (moc, rcc, uic) that execute during the build.
# Instrumenting them with sanitizers causes them to crash.
if(NOT PORT MATCHES "^qt")
    set(VCPKG_C_FLAGS_DEBUG   "${VCPKG_C_FLAGS_DEBUG} -O0 -g3 -fsanitize=address,undefined,leak")
    set(VCPKG_CXX_FLAGS_DEBUG "${VCPKG_CXX_FLAGS_DEBUG} -O0 -g3 -fsanitize=address,undefined,leak")
    set(VCPKG_LINKER_FLAGS_DEBUG "${VCPKG_LINKER_FLAGS_DEBUG} -fsanitize=address,undefined,leak")
endif()
