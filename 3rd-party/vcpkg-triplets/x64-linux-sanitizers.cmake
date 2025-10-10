set(VCPKG_TARGET_ARCHITECTURE x64)
set(VCPKG_CRT_LINKAGE dynamic)
set(VCPKG_LIBRARY_LINKAGE static)

set(VCPKG_CMAKE_SYSTEM_NAME Linux)

# Force debug build
# Force compilers
set(CMAKE_C_COMPILER clang)
set(CMAKE_CXX_COMPILER clang++)

set(VCPKG_C_FLAGS           "${VCPKG_C_FLAGS} -fno-omit-frame-pointer")
set(VCPKG_CXX_FLAGS         "${VCPKG_CXX_FLAGS} -fno-omit-frame-pointer")

set(VCPKG_C_FLAGS_DEBUG     "${VCPKG_C_FLAGS_DEBUG} -O0 -g3 -fno-omit-frame-pointer -fsanitize=address,undefined,leak -fsanitize-undefined-trap-on-error")
set(VCPKG_CXX_FLAGS_DEBUG   "${VCPKG_CXX_FLAGS_DEBUG} -O0 -g3 -fno-omit-frame-pointer -fsanitize=address,undefined,leak -fsanitize-undefined-trap-on-error")
set(VCPKG_LINKER_FLAGS_DEBUG "${VCPKG_LINKER_FLAGS_DEBUG} -fsanitize=address,undefined,leak -fsanitize-undefined-trap-on-error")
