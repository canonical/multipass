# Copyright (C) Canonical, Ltd.
#
# This program is free software: you can redistribute it and/or modify
# it under the terms of the GNU General Public License version 3 as
# published by the Free Software Foundation.
#
# This program is distributed in the hope that it will be useful,
# but WITHOUT ANY WARRANTY; without even the implied warranty of
# MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
# GNU General Public License for more details.
#
# You should have received a copy of the GNU General Public License
# along with this program.  If not, see <http://www.gnu.org/licenses/>.
#

set (CMAKE_AUTOMOC ON)

include(c_mock_defines.cmake)

# Find required packages
find_package(Qt6 COMPONENTS Core REQUIRED)

include(FetchContent)
set(FETCHCONTENT_QUIET FALSE)

FetchContent_Declare(googletest
  GIT_REPOSITORY https://github.com/google/googletest.git
  GIT_TAG main
  GIT_SHALLOW TRUE
  GIT_PROGRESS TRUE
)
set(gtest_force_shared_crt ON CACHE BOOL "" FORCE)
set(INSTALL_GTEST OFF CACHE BOOL "")
FetchContent_MakeAvailable(googletest)

# Link common dependencies to all object libraries 
set(TEST_COMMON_DEPS 
  Qt6::Core
  gmock
  gtest
)

# Common dependencies include paths
set(TEST_COMMON_INCLUDE_PATHS
  ${gtest_SOURCE_DIR}/googlemock/include
  ${gtest_SOURCE_DIR}/googletest/include
)

# Base include directories needed by all test components
set(BASE_INCLUDE_DIRS
  ${CMAKE_SOURCE_DIR}
  ${CMAKE_SOURCE_DIR}/src
  ${CMAKE_SOURCE_DIR}/include # For multipass headers
  ${CMAKE_SOURCE_DIR}/3rd-party/fmt/include # For fmt headers
  ${CMAKE_SOURCE_DIR}/3rd-party/scope_guard # For scope_guard.hpp
)

# Settings test object library
add_library(test_settings_obj OBJECT
  test_global_settings_handlers.cpp
  test_instance_settings_handler.cpp
  test_persistent_settings_handler.cpp
  test_remote_settings_handler.cpp
  test_setting_specs.cpp
  test_settings.cpp
)
target_include_directories(test_settings_obj PRIVATE
  ${BASE_INCLUDE_DIRS}
  ${TEST_COMMON_INCLUDE_PATHS}
  ${CMAKE_SOURCE_DIR}/src/settings
)
target_link_libraries(test_settings_obj PRIVATE 
  ${TEST_COMMON_DEPS}
  settings
)
target_compile_options(test_settings_obj PRIVATE -Wno-unused-parameter)

# Network and parser object library for tests needing Qt Network and protobuf
add_library(test_network_parser_obj OBJECT
  test_argparser.cpp
)
target_include_directories(test_network_parser_obj PRIVATE
  ${BASE_INCLUDE_DIRS}
  ${TEST_COMMON_INCLUDE_PATHS}
  ${CMAKE_BINARY_DIR}
  ${CMAKE_SOURCE_DIR}/3rd-party/yaml-cpp/include
)
target_link_libraries(test_network_parser_obj PRIVATE 
  ${TEST_COMMON_DEPS}
  Qt6::Network
  protobuf::libprotobuf
)
target_compile_options(test_network_parser_obj PRIVATE -Wno-unused-parameter)

# Core object libraries
add_library(test_minimal_obj OBJECT
  common.cpp
  main.cpp
  mock_logger.cpp
  mock_standard_paths.cpp
  temp_dir.cpp
  temp_file.cpp
)
target_include_directories(test_minimal_obj PRIVATE 
  ${BASE_INCLUDE_DIRS}
  ${TEST_COMMON_INCLUDE_PATHS}
)
target_link_libraries(test_minimal_obj PRIVATE ${TEST_COMMON_DEPS})
target_compile_options(test_minimal_obj PRIVATE -Wno-unused-parameter)

add_library(test_process_obj OBJECT
  mock_process_factory.cpp
  stub_process_factory.cpp
  reset_process_factory.cpp
)
target_include_directories(test_process_obj PRIVATE 
  ${BASE_INCLUDE_DIRS}
  ${TEST_COMMON_INCLUDE_PATHS}
  ${CMAKE_SOURCE_DIR}/src/platform/backends/shared/linux # For process_factory.h
)
target_link_libraries(test_process_obj PRIVATE ${TEST_COMMON_DEPS})
target_compile_options(test_process_obj PRIVATE -Wno-unused-parameter)

add_library(test_fs_obj OBJECT
  file_operations.cpp
  path.cpp
)
target_include_directories(test_fs_obj PRIVATE 
  ${BASE_INCLUDE_DIRS}
  ${TEST_COMMON_INCLUDE_PATHS}
  ${CMAKE_SOURCE_DIR}/3rd-party/yaml-cpp/include # For yaml-cpp headers
)
target_link_libraries(test_fs_obj PRIVATE ${TEST_COMMON_DEPS})

add_library(test_ssh_obj OBJECT
  mock_ssh.cpp
  mock_ssh_client.cpp
  mock_sftp.cpp
  mock_sftpserver.cpp
)
target_include_directories(test_ssh_obj PRIVATE 
  ${BASE_INCLUDE_DIRS}
  ${TEST_COMMON_INCLUDE_PATHS}
  ${CMAKE_SOURCE_DIR}/src/ssh
  ${CMAKE_SOURCE_DIR}/3rd-party/premock/ # For premock headers
)
target_link_libraries(test_ssh_obj PRIVATE ${TEST_COMMON_DEPS})

add_library(test_json_obj OBJECT
  json_test_utils.cpp
  blueprint_test_lambdas.cpp
)
target_include_directories(test_json_obj PRIVATE 
  ${BASE_INCLUDE_DIRS}
  ${TEST_COMMON_INCLUDE_PATHS}
  ${CMAKE_SOURCE_DIR}/3rd-party/yaml-cpp/include
  ${CMAKE_BINARY_DIR} # For generated protobuf headers
)
target_link_libraries(test_json_obj PRIVATE ${TEST_COMMON_DEPS} protobuf::libprotobuf)
target_compile_options(test_json_obj PRIVATE -Wno-unused-parameter)


add_library(test_network_obj OBJECT
  mischievous_url_downloader.cpp
  image_host_remote_count.cpp
)
target_include_directories(test_network_obj PRIVATE
  ${BASE_INCLUDE_DIRS}
  ${TEST_COMMON_INCLUDE_PATHS}
  ${CMAKE_SOURCE_DIR}/src/network
)
target_link_libraries(test_network_obj PRIVATE ${TEST_COMMON_DEPS} Qt6::Network)

add_library(test_daemon_obj OBJECT
  daemon_test_fixture.cpp
)
target_include_directories(test_daemon_obj PRIVATE
  ${BASE_INCLUDE_DIRS}
  ${TEST_COMMON_INCLUDE_PATHS}
  ${CMAKE_SOURCE_DIR}/src/daemon
  ${CMAKE_SOURCE_DIR}/src/platform/backends
  ${CMAKE_SOURCE_DIR}/src/platform/backends/shared
  ${CMAKE_SOURCE_DIR}/3rd-party/premock/ # For premock headers
  ${CMAKE_SOURCE_DIR}/3rd-party/yaml-cpp/include
  ${CMAKE_SOURCE_DIR}/3rd-party/xz-decoder/xz-embedded/linux/include/linux # For xz.h
  ${CMAKE_BINARY_DIR} # For generated protobuf headers
)
target_link_libraries(test_daemon_obj PRIVATE ${TEST_COMMON_DEPS} protobuf::libprotobuf Qt6::Network)
target_compile_options(test_daemon_obj PRIVATE -Wno-unused-parameter)

# Move OpenSSL mocks to their own object library
add_library(test_openssl_obj OBJECT
  mock_openssl_syscalls.cpp # For OpenSSL mocks including ut_EVP_PBE_scrypt
)
target_include_directories(test_openssl_obj PRIVATE
  ${BASE_INCLUDE_DIRS}
  ${TEST_COMMON_INCLUDE_PATHS}
  ${CMAKE_SOURCE_DIR}/3rd-party/premock/ # For premock headers

)
target_link_libraries(test_openssl_obj PRIVATE ${TEST_COMMON_DEPS})

# Common source combinations using object libraries
set(MINIMAL_TEST_OBJECTS
  $<TARGET_OBJECTS:test_minimal_obj>
  $<TARGET_OBJECTS:test_openssl_obj>
)

set(NETWORK_TEST_OBJECTS
  ${MINIMAL_TEST_OBJECTS}
  $<TARGET_OBJECTS:test_network_obj>
  $<TARGET_OBJECTS:test_fs_obj>
)

set(SSH_CLIENT_TEST_OBJECTS
  ${MINIMAL_TEST_OBJECTS}
  $<TARGET_OBJECTS:test_process_obj>
  $<TARGET_OBJECTS:test_fs_obj>
  $<TARGET_OBJECTS:test_ssh_obj>
  $<TARGET_OBJECTS:test_json_obj>
  $<TARGET_OBJECTS:test_daemon_obj>
)

set(DAEMON_TEST_OBJECTS
  ${MINIMAL_TEST_OBJECTS}
  $<TARGET_OBJECTS:test_process_obj>
  $<TARGET_OBJECTS:test_fs_obj>
  $<TARGET_OBJECTS:test_ssh_obj>
  $<TARGET_OBJECTS:test_json_obj>
  $<TARGET_OBJECTS:test_daemon_obj>
  $<TARGET_OBJECTS:test_network_obj>
)

set(QEMU_TEST_OBJECTS
  ${MINIMAL_TEST_OBJECTS}
  $<TARGET_OBJECTS:test_process_obj>
  $<TARGET_OBJECTS:test_fs_obj>
  $<TARGET_OBJECTS:test_ssh_obj>
  $<TARGET_OBJECTS:test_json_obj>
  $<TARGET_OBJECTS:test_daemon_obj>
  $<TARGET_OBJECTS:test_network_obj>
)

set(COMMON_TEST_OBJECTS
  ${MINIMAL_TEST_OBJECTS}
  $<TARGET_OBJECTS:test_process_obj>
  $<TARGET_OBJECTS:test_fs_obj>
  $<TARGET_OBJECTS:test_ssh_obj>
  $<TARGET_OBJECTS:test_json_obj>
  $<TARGET_OBJECTS:test_daemon_obj>
  $<TARGET_OBJECTS:test_network_obj>
  $<TARGET_OBJECTS:test_network_parser_obj>
  $<TARGET_OBJECTS:test_settings_obj>
)

# SSH test sources
set(SSH_TEST_SOURCES
  test_sftp_client.cpp
  test_sftpserver.cpp
  test_ssh_client.cpp
  test_ssh_exec_failure.cpp
  test_ssh_key_provider.cpp
  test_ssh_process.cpp
  test_ssh_session.cpp
  test_sshfs_server_process_spec.cpp
  test_sshfsmount.cpp
  test_sshfs_mount_handler.cpp
  test_sftp_dir_iterator.cpp
  test_sftp_utils.cpp
)

# Memory test sources
set(MEMORY_TEST_SOURCES
  test_memory_size.cpp
  test_constants.cpp
)

# Formatter test sources
set(FORMATTER_TEST_SOURCES
  test_format_utils.cpp
  test_output_formatter.cpp
  test_id_mappings.cpp
  test_log.cpp
  test_standard_logger.cpp
  test_top_catch_all.cpp
  test_exception.cpp
)

# Network test sources
set(NETWORK_TEST_SOURCES
  test_url_downloader.cpp
  test_ip_address.cpp
  test_ubuntu_image_host.cpp
  test_custom_image_host.cpp
  test_new_release_monitor.cpp
)

# Client test sources
set(CLIENT_TEST_SOURCES
  test_cli_client.cpp
  test_cli_prompters.cpp
  test_client_cert_store.cpp
  test_client_common.cpp
  test_client_logger.cpp
  test_alias_dict.cpp
)

# QEMU test sources
set(QEMU_TEST_SOURCES
  qemu/test_qemu_backend.cpp
  qemu/test_qemu_img_utils.cpp
  qemu/test_qemu_mount_handler.cpp
  qemu/test_qemu_snapshot.cpp
  qemu/test_qemu_vm_process_spec.cpp
  qemu/test_qemu_vmstate_process_spec.cpp
  qemu/linux/test_dnsmasq_process_spec.cpp
  qemu/linux/test_dnsmasq_server.cpp
  qemu/linux/test_firewall_config.cpp
  qemu/linux/test_qemu_platform_detail.cpp
)

# Daemon test executable
add_executable(multipass_daemon_tests
  ${DAEMON_TEST_OBJECTS}
  test_daemon.cpp
  test_daemon_authenticate.cpp
  test_daemon_clone.cpp
  test_daemon_find.cpp
  test_daemon_launch.cpp
  test_daemon_mount.cpp
  test_daemon_snapshot_restore.cpp
  test_daemon_start.cpp
  test_daemon_suspend.cpp
  test_daemon_umount.cpp
  unix/test_daemon_rpc.cpp
)

# Client and SSH test executable
add_executable(multipass_client_ssh_tests
  ${SSH_CLIENT_TEST_OBJECTS}
  ${SSH_TEST_SOURCES}
  ${CLIENT_TEST_SOURCES}
)

# Memory test executable
add_executable(multipass_memory_tests
  ${MINIMAL_TEST_OBJECTS}
  ${MEMORY_TEST_SOURCES}
)

# Formatter test executable
add_executable(multipass_formatter_tests
  ${MINIMAL_TEST_OBJECTS}
  ${FORMATTER_TEST_SOURCES}
)

# Network test executable
add_executable(multipass_network_tests
  ${NETWORK_TEST_OBJECTS}
  ${NETWORK_TEST_SOURCES}
)

# QEMU test executable
add_executable(multipass_qemu_tests
  ${QEMU_TEST_OBJECTS}
  ${QEMU_TEST_SOURCES}
  test_with_mocked_bin_path.cpp
)


# Main tests executable with remaining tests that need full dependencies
add_executable(multipass_tests
  ${COMMON_TEST_OBJECTS}
  test_base_snapshot.cpp
  test_base_virtual_machine.cpp
  test_base_virtual_machine_factory.cpp
  test_basic_process.cpp
  test_blueprint_provider.cpp
  test_cloud_init_iso.cpp
  test_common_callbacks.cpp
  test_file_ops.cpp
  test_image_vault.cpp
  test_image_vault_utils.cpp
  test_json_utils.cpp
  test_mock_standard_paths.cpp
  test_mount_handler.cpp
  test_petname.cpp
  test_private_pass_provider.cpp
  test_qemuimg_process_spec.cpp
  test_recursive_dir_iter.cpp
  test_simple_streams_index.cpp
  test_simple_streams_manifest.cpp
  test_ssl_cert_provider.cpp
  test_utils.cpp
  test_vm_mount.cpp
  test_with_mocked_bin_path.cpp # needed for test_platform_linux
  test_yaml_node_utils.cpp
  test_timer.cpp
  #No deps
  test_disabled_copy_move.cpp
  test_singleton.cpp
)

# Set include directories for each test suite based on their needs
target_include_directories(multipass_memory_tests PRIVATE
  ${BASE_INCLUDE_DIRS}
  ${CMAKE_SOURCE_DIR}/src/network # For IP address and network testing
)

target_include_directories(multipass_formatter_tests PRIVATE
  ${BASE_INCLUDE_DIRS}
  ${CMAKE_SOURCE_DIR}/src/settings # For settings management
)

target_include_directories(multipass_network_tests PRIVATE
  ${BASE_INCLUDE_DIRS}
  ${CMAKE_SOURCE_DIR}/src/network
  ${CMAKE_SOURCE_DIR}/src/settings
)

target_include_directories(multipass_client_ssh_tests PRIVATE
  ${BASE_INCLUDE_DIRS}
  ${CMAKE_SOURCE_DIR}/src/ssh
  ${CMAKE_SOURCE_DIR}/src/sshfs_mount
  ${CMAKE_SOURCE_DIR}/src/client
  ${CMAKE_SOURCE_DIR}/src/network
  ${CMAKE_SOURCE_DIR}/src/daemon
  ${CMAKE_SOURCE_DIR}/src/platform/backends
  ${CMAKE_SOURCE_DIR}/src/platform/backends/shared # For base VM classes
)

target_include_directories(multipass_daemon_tests PRIVATE
  ${BASE_INCLUDE_DIRS}
  ${CMAKE_SOURCE_DIR}/src/platform/backends
  ${CMAKE_SOURCE_DIR}/src/daemon
  ${CMAKE_SOURCE_DIR}/src/ssh
  ${CMAKE_SOURCE_DIR}/src/network
)

target_include_directories(multipass_qemu_tests PRIVATE
  ${BASE_INCLUDE_DIRS}
  ${CMAKE_SOURCE_DIR}/src/platform/backends
  ${CMAKE_SOURCE_DIR}/src/platform/backends/qemu
  ${CMAKE_SOURCE_DIR}/src/platform/backends/qemu/linux
  ${CMAKE_SOURCE_DIR}/src/network
  ${CMAKE_SOURCE_DIR}/src/ssh
)

target_include_directories(multipass_tests PRIVATE
  ${BASE_INCLUDE_DIRS}
  ${CMAKE_SOURCE_DIR}/src/platform/backends # For generic backend code
  ${CMAKE_SOURCE_DIR}/src/network # For base VM networking
  ${CMAKE_SOURCE_DIR}/src/ssh # For SSH operations
  ${CMAKE_SOURCE_DIR}/src/settings # For settings tests
)

add_definitions(-DWITH_SERVER)
target_compile_definitions(ssh_test PRIVATE
  ${c_mock_defines})
target_compile_definitions(sshfs_mount_test PRIVATE
  ${c_mock_defines})
target_compile_definitions(sftp_test PRIVATE
  ${c_mock_defines})
target_compile_definitions(ssh_client_test PRIVATE
  ${c_mock_defines})

target_compile_definitions(utils_test PRIVATE
  -DEVP_PBE_scrypt=ut_EVP_PBE_scrypt)

# Base libraries needed by all tests
set(BASE_TEST_LIBRARIES
  client # For shared functionality
  gmock_main # For mocking
  gtest_main # For testing framework
  premock # For mocking
  scope_guard # For cleanup
  utils_test # For utility functions
)

# Minimal networking libraries
set(MEMORY_TEST_LIBRARIES
  ${BASE_TEST_LIBRARIES}
  ip_address # For IP validation
  network # Core network functionality
)

# Libraries for configuration management
set(FORMATTER_TEST_LIBRARIES
  ${BASE_TEST_LIBRARIES}
  settings # For settings management
  yaml # For YAML formatting
)

# Libraries for network operations and configuration
set(NETWORK_TEST_LIBRARIES
  ${MEMORY_TEST_LIBRARIES}
  settings # For configuration
)

# Full set of libraries for VM/daemon operations
set(DAEMON_TEST_LIBRARIES
  ${NETWORK_TEST_LIBRARIES}
  daemon # Core daemon functionality
  ssh_test # For SSH operations
  yaml # For config parsing
)

# Libraries for client and SSH tests (extends daemon libraries)
set(CLIENT_SSH_TEST_LIBRARIES
  ${DAEMON_TEST_LIBRARIES}
  sftp_test # For SFTP tests
  ssh_client_test # For SSH client tests
  sshfs_mount_test # For SFTP server tests
)

target_link_libraries(multipass_daemon_tests ${DAEMON_TEST_LIBRARIES})
target_link_libraries(multipass_client_ssh_tests ${CLIENT_SSH_TEST_LIBRARIES})
target_link_libraries(multipass_memory_tests ${MEMORY_TEST_LIBRARIES}) 
target_link_libraries(multipass_formatter_tests ${FORMATTER_TEST_LIBRARIES})
target_link_libraries(multipass_network_tests ${NETWORK_TEST_LIBRARIES})
target_link_libraries(multipass_qemu_tests ${DAEMON_TEST_LIBRARIES})
target_link_libraries(multipass_tests ${DAEMON_TEST_LIBRARIES})

# need https://github.com/google/googletest/commit/a09ea700d32bab83325aff9ff34d0582e50e3997 before this can be removed
if (CMAKE_CXX_COMPILER_ID STREQUAL "Clang")
  target_compile_options(multipass_tests PRIVATE -Wno-inconsistent-missing-override)
  target_compile_options(multipass_daemon_tests PRIVATE -Wno-inconsistent-missing-override)
  target_compile_options(multipass_client_ssh_tests PRIVATE -Wno-inconsistent-missing-override)
  target_compile_options(multipass_memory_tests PRIVATE -Wno-inconsistent-missing-override)
  target_compile_options(multipass_formatter_tests PRIVATE -Wno-inconsistent-missing-override)
  target_compile_options(multipass_network_tests PRIVATE -Wno-inconsistent-missing-override)
  target_compile_options(multipass_qemu_tests PRIVATE -Wno-inconsistent-missing-override)
endif()

add_test(NAME multipass_daemon_tests COMMAND multipass_daemon_tests)
add_test(NAME multipass_client_ssh_tests COMMAND multipass_client_ssh_tests)
add_test(NAME multipass_memory_tests COMMAND multipass_memory_tests)
add_test(NAME multipass_formatter_tests COMMAND multipass_formatter_tests)
add_test(NAME multipass_network_tests COMMAND multipass_network_tests)
add_test(NAME multipass_qemu_tests COMMAND multipass_qemu_tests)
add_test(NAME multipass_tests COMMAND multipass_tests)

foreach(BACKEND IN LISTS MULTIPASS_BACKENDS)
  string(TOUPPER ${BACKEND}_ENABLED DEF)
  target_compile_definitions(multipass_tests PRIVATE ${DEF})
  target_compile_definitions(multipass_daemon_tests PRIVATE ${DEF})
  target_compile_definitions(multipass_client_ssh_tests PRIVATE ${DEF})
  target_compile_definitions(multipass_memory_tests PRIVATE ${DEF})
  target_compile_definitions(multipass_formatter_tests PRIVATE ${DEF})
  target_compile_definitions(multipass_network_tests PRIVATE ${DEF})
  target_compile_definitions(multipass_qemu_tests PRIVATE ${DEF})
endforeach()

string(TOLOWER ${CMAKE_HOST_SYSTEM_PROCESSOR} MANIFEST_ARCH)
if (${MANIFEST_ARCH} STREQUAL "x86_64")
  string(TOLOWER "amd64" MANIFEST_ARCH)
endif()

target_compile_definitions(multipass_tests PRIVATE -DMANIFEST_ARCH="${MANIFEST_ARCH}")
target_compile_definitions(multipass_daemon_tests PRIVATE -DMANIFEST_ARCH="${MANIFEST_ARCH}")
target_compile_definitions(multipass_client_ssh_tests PRIVATE -DMANIFEST_ARCH="${MANIFEST_ARCH}")
target_compile_definitions(multipass_memory_tests PRIVATE -DMANIFEST_ARCH="${MANIFEST_ARCH}")
target_compile_definitions(multipass_formatter_tests PRIVATE -DMANIFEST_ARCH="${MANIFEST_ARCH}")
target_compile_definitions(multipass_network_tests PRIVATE -DMANIFEST_ARCH="${MANIFEST_ARCH}")
target_compile_definitions(multipass_qemu_tests PRIVATE -DMANIFEST_ARCH="${MANIFEST_ARCH}")

file(GLOB_RECURSE MANIFEST_FILES RELATIVE "${CMAKE_CURRENT_SOURCE_DIR}" "test_data/*.in")

foreach (MANIFEST_IN IN LISTS MANIFEST_FILES)
  string(REGEX REPLACE "\.in$" "" MANIFEST_OUT ${MANIFEST_IN})
  configure_file(
    ${MANIFEST_IN}
    ${CMAKE_RUNTIME_OUTPUT_DIRECTORY}/${MANIFEST_OUT}
    @ONLY)
endforeach()

file(COPY test_data DESTINATION ${CMAKE_RUNTIME_OUTPUT_DIRECTORY} PATTERN "*\.in" EXCLUDE)

# Mock binaries for testing
add_executable(mock_process
  mock_process.cpp)

add_executable(mock_qemuimg
  qemu/mock_qemuimg.cpp)

set_target_properties(mock_process mock_qemuimg
  PROPERTIES
  RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/bin/mocks"
  RUNTIME_OUTPUT_DIRECTORY_DEBUG "${CMAKE_BINARY_DIR}/bin/mocks"
  RUNTIME_OUTPUT_DIRECTORY_RELEASE "${CMAKE_BINARY_DIR}/bin/mocks")

add_dependencies(multipass_tests mock_process mock_qemuimg)
add_dependencies(multipass_daemon_tests mock_process mock_qemuimg)
add_dependencies(multipass_client_ssh_tests mock_process mock_qemuimg)
add_dependencies(multipass_memory_tests mock_process mock_qemuimg)
add_dependencies(multipass_formatter_tests mock_process mock_qemuimg)
add_dependencies(multipass_network_tests mock_process mock_qemuimg)
add_dependencies(multipass_qemu_tests mock_process mock_qemuimg)

# Add platform include directory
target_include_directories(multipass_tests BEFORE
  PRIVATE ${CMAKE_SOURCE_DIR}/src/platform/backends/shared/${MULTIPASS_PLATFORM}
)
target_include_directories(multipass_daemon_tests BEFORE
  PRIVATE ${CMAKE_SOURCE_DIR}/src/platform/backends/shared/${MULTIPASS_PLATFORM}
)
target_include_directories(multipass_client_ssh_tests BEFORE
  PRIVATE ${CMAKE_SOURCE_DIR}/src/platform/backends/shared/${MULTIPASS_PLATFORM}
)
target_include_directories(multipass_memory_tests BEFORE
  PRIVATE ${CMAKE_SOURCE_DIR}/src/platform/backends/shared/${MULTIPASS_PLATFORM}
)
target_include_directories(multipass_formatter_tests BEFORE
  PRIVATE ${CMAKE_SOURCE_DIR}/src/platform/backends/shared/${MULTIPASS_PLATFORM}
)
target_include_directories(multipass_network_tests BEFORE
  PRIVATE ${CMAKE_SOURCE_DIR}/src/platform/backends/shared/${MULTIPASS_PLATFORM}
)
target_include_directories(multipass_qemu_tests BEFORE
  PRIVATE ${CMAKE_SOURCE_DIR}/src/platform/backends/shared/${MULTIPASS_PLATFORM}
)

add_subdirectory(${MULTIPASS_PLATFORM})

foreach(BACKEND IN LISTS MULTIPASS_BACKENDS)
  add_subdirectory(${BACKEND})
endforeach()

if (UNIX)
  add_subdirectory(unix)
endif()
