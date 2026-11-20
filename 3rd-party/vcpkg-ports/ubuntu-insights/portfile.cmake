set(VCPKG_POLICY_MISMATCHED_NUMBER_OF_BINARIES enabled)
vcpkg_check_linkage(ONLY_DYNAMIC_LIBRARY)

# Using patchelf on the server tools causes them to crash
set(VCPKG_FIXUP_ELF_RPATH OFF)

if(VCPKG_TARGET_IS_LINUX)
    set(INSIGHTS_LIBRARY libinsights.so.1)
elseif(VCPKG_TARGET_IS_OSX)
    set(INSIGHTS_LIBRARY libinsights.1.dylib)
elseif(VCPKG_TARGET_IS_WINDOWS)
    if(NOT VCPKG_TARGET_ARCHITECTURE STREQUAL "x64")
        message(FATAL_ERROR "ubuntu-insights on Windows requires x64 (the pinned MinGW closure is x86_64-only).")
    endif()
    set(INSIGHTS_LIBRARY libinsights.dll)
else()
    message(FATAL_ERROR "ubuntu-insights supports Linux, macOS, and Windows targets.")
endif()

vcpkg_from_github(
    OUT_SOURCE_PATH SOURCE_PATH
    REPO ubuntu/ubuntu-insights
    REF "insights/v${VERSION}"
    SHA512 54420dd09b3fa4b96a989214fbef649e2e294149d8a9ed9638cec7eac55b8f49a572111376873eaee155cab0198ecfed846e8124f97a736062b22afd23917a6f
    PATCHES 0001-c-api-free.patch
)

# Ideally we should use `vcpkg_find_acquire_program` to automatically acquire go
# if it doesn't exist. Unfortunately the version provided by vcpkg (1.25) is less
# than the one required by ubuntu-insights (1.26).
find_program(GO_EXECUTABLE NAMES go HINTS ENV GO_EXECUTABLE_DIR REQUIRED)

# Set up the common go environment that will be used to build the tools and the C-API
set(GO_ENV
    CGO_ENABLED=1
    # An unset CGO_CFLAGS makes the upstream generator add -Werror and pedantic warnings
    "CGO_CFLAGS=-O2 -g"
    # Read by insights/C/generate to stamp the library version (defaults to "Dev")
    "DEB_VERSION_UPSTREAM=${VERSION}"
)
if(VCPKG_TARGET_IS_OSX AND VCPKG_OSX_DEPLOYMENT_TARGET)
    list(APPEND GO_ENV "MACOSX_DEPLOYMENT_TARGET=${VCPKG_OSX_DEPLOYMENT_TARGET}")
endif()

# If on Windows, acquire MinGW which is required for building the C API.
#
# TODO: The acquired MinGW targets the legacy C runtime (CRT). Update it to use
# the UCRT packages instead. For now this should be ok.
if(VCPKG_TARGET_IS_WINDOWS)
    include("${CMAKE_CURRENT_LIST_DIR}/../../vcpkg-port-utils/mingw/acquire-mingw-msys.cmake")
    multipass_acquire_mingw_msys(MSYS_ROOT)
    file(TO_NATIVE_PATH "${MSYS_ROOT}/mingw64/bin" MINGW_BIN)
    list(APPEND GO_ENV
        "CC=${MINGW_BIN}/gcc.exe"
        --modify "PATH=path_list_prepend:${MINGW_BIN}"
    )

    set(C_API_ENV ${GO_ENV})
    set(INSIGHTS_DEF "${CURRENT_BUILDTREES_DIR}/${TARGET_TRIPLET}-insights.def")
    list(APPEND C_API_ENV "CGO_LDFLAGS=-Wl,--output-def,${INSIGHTS_DEF}")
else()
    set(C_API_ENV ${GO_ENV})
endif()

set(INSIGHTS_TOOLS "")

# Build the tools if the feature is requested
if("tools" IN_LIST FEATURES)
    function(add_insights_tool tool package)
        vcpkg_execute_required_process(
            COMMAND
                "${CMAKE_COMMAND}" -E env ${GO_ENV}
                "${GO_EXECUTABLE}" build -trimpath
                -o "${tools_dir}/${tool}${VCPKG_TARGET_EXECUTABLE_SUFFIX}" "${package}"
            WORKING_DIRECTORY "${SOURCE_PATH}"
            LOGNAME "build-${tool}"
        )
        list(APPEND INSIGHTS_TOOLS "${tool}")
        set(INSIGHTS_TOOLS "${INSIGHTS_TOOLS}" PARENT_SCOPE)
    endfunction()

    set(tools_dir "${CURRENT_PACKAGES_DIR}/tools/${PORT}")
    file(MAKE_DIRECTORY "${tools_dir}")

    add_insights_tool(client ./insights/cmd/insights)
    if(VCPKG_TARGET_IS_LINUX)
        add_insights_tool(web-service ./server/cmd/web-service)
        add_insights_tool(ingest-service ./server/cmd/ingest-service)
    endif()
endif()

# Build the C-API
vcpkg_execute_required_process(
    COMMAND
        "${CMAKE_COMMAND}" -E env ${C_API_ENV}
        "${GO_EXECUTABLE}" run -tags=tools ./generate/...
    WORKING_DIRECTORY "${SOURCE_PATH}/insights/C"
    LOGNAME build-c-api
)

if(VCPKG_TARGET_IS_WINDOWS)
    # Generate an import library, otherwise MSVC will not be able to link against the generated dll
    file(MAKE_DIRECTORY "${CURRENT_PACKAGES_DIR}/lib")
    set(INSIGHTS_IMPLIB libinsights.lib)
    find_program(lib_tool NAMES lib.exe llvm-lib REQUIRED)
    vcpkg_execute_required_process(
        COMMAND "${lib_tool}" "/def:${INSIGHTS_DEF}" /machine:x64
                "/out:${CURRENT_PACKAGES_DIR}/lib/${INSIGHTS_IMPLIB}"
        WORKING_DIRECTORY "${CURRENT_BUILDTREES_DIR}"
        LOGNAME import-library
    )

    file(INSTALL     "${SOURCE_PATH}/insights/generated/${INSIGHTS_LIBRARY}"
         DESTINATION "${CURRENT_PACKAGES_DIR}/bin")

    # libinsights is built with a single configuration, but vcpkg's applocal
    # searches only debug/bin for DLLs when building Debug executables.
    # Copy the DLL there as well so it can be deployed automatically.
    #
    # We also copy the lib import library so that vcpkg doesn't issue a warning
    # for a dll without an accompany import library.
    file(INSTALL     "${SOURCE_PATH}/insights/generated/${INSIGHTS_LIBRARY}"
         DESTINATION "${CURRENT_PACKAGES_DIR}/debug/bin")
    file(INSTALL     "${CURRENT_PACKAGES_DIR}/lib/${INSIGHTS_IMPLIB}"
         DESTINATION "${CURRENT_PACKAGES_DIR}/debug/lib/")
else()
    file(INSTALL     "${SOURCE_PATH}/insights/generated/${INSIGHTS_LIBRARY}"
         DESTINATION "${CURRENT_PACKAGES_DIR}/lib")
endif()

file(INSTALL
    "${SOURCE_PATH}/insights/generated/insights.h"
    "${SOURCE_PATH}/insights/generated/types.h"
    DESTINATION "${CURRENT_PACKAGES_DIR}/include/insights")

configure_file("${CMAKE_CURRENT_LIST_DIR}/ubuntu-insights-config.cmake.in"
    "${CURRENT_PACKAGES_DIR}/share/${PORT}/ubuntu-insights-config.cmake" @ONLY)

include(CMakePackageConfigHelpers)
write_basic_package_version_file(
    "${CURRENT_PACKAGES_DIR}/share/${PORT}/ubuntu-insights-config-version.cmake"
    VERSION "${VERSION}"
    COMPATIBILITY SameMajorVersion
)

if("tools" IN_LIST FEATURES)
    vcpkg_copy_tool_dependencies("${tools_dir}")
endif()
vcpkg_install_copyright(FILE_LIST "${SOURCE_PATH}/LICENSE")

file(INSTALL "${CMAKE_CURRENT_LIST_DIR}/usage" DESTINATION "${CURRENT_PACKAGES_DIR}/share/${PORT}")
