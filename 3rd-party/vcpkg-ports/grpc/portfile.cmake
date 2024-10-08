if(VCPKG_TARGET_IS_WINDOWS)
    vcpkg_check_linkage(ONLY_STATIC_LIBRARY)
endif()

vcpkg_from_github(
    OUT_SOURCE_PATH SOURCE_PATH
    REPO canonical/grpc
    REF 0d3d5144330b7584621aedf3bd1be47d9813bea7
    SHA512 a8a9d11f31538007f53a2a27dab09ee1a641c05b2f8c08b9889429f54d8801319224612360da35b20fc9e9392deff9ae18a2fec1b8862d22d0d432a5bc83be33
    HEAD_REF master
    PATCHES
        00001-fix-uwp.patch
        00002-static-linking-in-linux.patch
        00003-undef-base64-macro.patch
        00004-link-gdi32-on-windows.patch
        00005-fix-uwp-error.patch
        00009-use-system-upb.patch
        00015-disable-download-archive.patch
        00016-fix-plugin-targets.patch
        00017-abseil.patch
)

# Ensure de-vendoring
file(REMOVE_RECURSE
    "${SOURCE_PATH}/third_party/abseil-cpp"
    "${SOURCE_PATH}/third_party/cares"
    "${SOURCE_PATH}/third_party/protobuf"
    "${SOURCE_PATH}/third_party/re2"
    "${SOURCE_PATH}/third_party/upb"
    "${SOURCE_PATH}/third_party/utf8_range"
    "${SOURCE_PATH}/third_party/zlib"
)

if(VCPKG_CROSSCOMPILING)
    vcpkg_add_to_path(PREPEND "${CURRENT_HOST_INSTALLED_DIR}/tools/grpc")
endif()

string(COMPARE EQUAL "${VCPKG_CRT_LINKAGE}" "static" gRPC_MSVC_STATIC_RUNTIME)
string(COMPARE EQUAL "${VCPKG_LIBRARY_LINKAGE}" "static" gRPC_STATIC_LINKING)

if(VCPKG_TARGET_IS_UWP)
    set(cares_CARES_PROVIDER OFF)
else()
    set(cares_CARES_PROVIDER "package")
endif()

vcpkg_check_features(
    OUT_FEATURE_OPTIONS FEATURE_OPTIONS
    FEATURES
        codegen gRPC_BUILD_CODEGEN
)

vcpkg_cmake_configure(
    SOURCE_PATH "${SOURCE_PATH}"
    OPTIONS ${FEATURE_OPTIONS}
        -DgRPC_INSTALL=ON
        -DgRPC_BUILD_TESTS=OFF
        -DgRPC_STATIC_LINKING=${gRPC_STATIC_LINKING}
        -DgRPC_MSVC_STATIC_RUNTIME=${gRPC_MSVC_STATIC_RUNTIME}
        -DgRPC_ZLIB_PROVIDER=package
        -DgRPC_SSL_PROVIDER=package
        -DgRPC_PROTOBUF_PROVIDER=package
        -DgRPC_ABSL_PROVIDER=package
        -DgRPC_RE2_PROVIDER=package
        -DgRPC_CARES_PROVIDER=${cares_CARES_PROVIDER}
        -DgRPC_BENCHMARK_PROVIDER=none
        -DgRPC_INSTALL_BINDIR:STRING=bin
        -DgRPC_INSTALL_LIBDIR:STRING=lib
        -DgRPC_INSTALL_INCLUDEDIR:STRING=include
        -DgRPC_INSTALL_CMAKEDIR:STRING=share/grpc
        -DgRPC_BUILD_GRPC_CSHARP_PLUGIN=OFF
        -DgRPC_BUILD_GRPC_NODE_PLUGIN=OFF
        -DgRPC_BUILD_GRPC_OBJECTIVE_C_PLUGIN=OFF
        -DgRPC_BUILD_GRPC_PHP_PLUGIN=OFF
        -DgRPC_BUILD_GRPC_PYTHON_PLUGIN=OFF
        -DgRPC_BUILD_GRPC_RUBY_PLUGIN=OFF
        "-D_gRPC_PROTOBUF_PROTOC_EXECUTABLE=${CURRENT_HOST_INSTALLED_DIR}/tools/protobuf/protoc${VCPKG_HOST_EXECUTABLE_SUFFIX}"
        "-DProtobuf_PROTOC_EXECUTABLE=${CURRENT_HOST_INSTALLED_DIR}/tools/protobuf/protoc${VCPKG_HOST_EXECUTABLE_SUFFIX}"
    MAYBE_UNUSED_VARIABLES
        gRPC_MSVC_STATIC_RUNTIME
)

vcpkg_cmake_install(ADD_BIN_TO_PATH)

vcpkg_cmake_config_fixup()

vcpkg_copy_tools(
    AUTO_CLEAN
    TOOL_NAMES
        grpc_cpp_plugin
)

file(REMOVE_RECURSE "${CURRENT_PACKAGES_DIR}/debug/share" "${CURRENT_PACKAGES_DIR}/debug/include")

vcpkg_copy_pdbs()
if (VCPKG_TARGET_IS_WINDOWS)
    file(REMOVE_RECURSE "${CURRENT_PACKAGES_DIR}/lib/pkgconfig" "${CURRENT_PACKAGES_DIR}/debug/lib/pkgconfig")
else()
    vcpkg_fixup_pkgconfig(SKIP_CHECK)
endif()

vcpkg_install_copyright(FILE_LIST "${SOURCE_PATH}/LICENSE")
