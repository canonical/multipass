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

# Set compiler flags early
if(MSVC)
  add_definitions(-DGTEST_LANG_CXX11=1)
  add_compile_options(/bigobj)
elseif(APPLE)
  set(CMAKE_CXX_FLAGS "-Wno-inconsistent-missing-override")
  add_definitions(-DHOST_ARCH="${HOST_ARCH}")
endif()

# Global include directories for all test targets
include_directories(
  ${gtest_SOURCE_DIR}/googlemock/include
  ${gtest_SOURCE_DIR}/googletest/include
  ${CMAKE_SOURCE_DIR}
  ${CMAKE_SOURCE_DIR}/src
  ${CMAKE_SOURCE_DIR}/src/ssh
  ${CMAKE_SOURCE_DIR}/src/sshfs_mount
  ${CMAKE_SOURCE_DIR}/include # For multipass headers
  ${CMAKE_SOURCE_DIR}/3rd-party/fmt/include
  ${CMAKE_SOURCE_DIR}/3rd-party/scope_guard
  ${CMAKE_SOURCE_DIR}/3rd-party/yaml-cpp/include
  ${CMAKE_SOURCE_DIR}/3rd-party/premock
  ${CMAKE_SOURCE_DIR}/src/daemon
  ${CMAKE_SOURCE_DIR}/src/network
  ${CMAKE_SOURCE_DIR}/src/platform/backends
  ${CMAKE_SOURCE_DIR}/src/platform/backends/shared/${MULTIPASS_PLATFORM} # For platform-specific headers
  ${CMAKE_BINARY_DIR}
)

# Common test dependencies as an INTERFACE library
add_library(test_common_deps INTERFACE)
target_link_libraries(test_common_deps INTERFACE
  Qt6::Core
  Qt6::Network
  ssh_test
  sftp_test
  ssh_client_test
  sshfs_mount_test
  gmock
  gtest
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
target_include_directories(test_settings_obj PRIVATE ${CMAKE_SOURCE_DIR}/src/settings)
target_link_libraries(test_settings_obj PRIVATE
  test_common_deps
  settings
)

# Network and parser object library for tests needing protobuf
add_library(test_network_parser_obj OBJECT
  test_argparser.cpp
)
target_link_libraries(test_network_parser_obj PRIVATE
  test_common_deps
  protobuf::libprotobuf
  client
  daemon
)

# Core object libraries
add_library(test_minimal_obj OBJECT
  common.cpp
  main.cpp
  mock_logger.cpp
  mock_standard_paths.cpp
  temp_dir.cpp
  temp_file.cpp
)
target_link_libraries(test_minimal_obj PRIVATE test_common_deps)

add_library(test_fs_obj OBJECT
  file_operations.cpp
  path.cpp
)
target_link_libraries(test_fs_obj PRIVATE test_common_deps)

add_library(test_json_obj OBJECT
  json_test_utils.cpp
  blueprint_test_lambdas.cpp
)
target_link_libraries(test_json_obj PRIVATE test_common_deps protobuf::libprotobuf)

add_library(test_network_obj OBJECT
  mischievous_url_downloader.cpp
  image_host_remote_count.cpp
)
target_link_libraries(test_network_obj PRIVATE test_common_deps)

add_library(test_daemon_obj OBJECT
  daemon_test_fixture.cpp
)
target_link_libraries(test_daemon_obj PRIVATE test_common_deps protobuf::libprotobuf daemon)

add_library(test_openssl_obj OBJECT
  mock_openssl_syscalls.cpp
)
target_link_libraries(test_openssl_obj PRIVATE test_common_deps)

# Process and SSH files that were causing issues as object libraries
set(TEST_PROCESS_SOURCES
  mock_process_factory.cpp
  stub_process_factory.cpp
  reset_process_factory.cpp
)

set(TEST_SSH_SOURCES
  mock_ssh.cpp
  mock_ssh_client.cpp
  mock_sftp.cpp
  mock_sftpserver.cpp
)

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
  ${TEST_PROCESS_SOURCES}
  $<TARGET_OBJECTS:test_fs_obj>
  ${TEST_SSH_SOURCES}
  $<TARGET_OBJECTS:test_json_obj>
  $<TARGET_OBJECTS:test_daemon_obj>
)

set(DAEMON_TEST_OBJECTS
  ${MINIMAL_TEST_OBJECTS}
  ${TEST_PROCESS_SOURCES}
  $<TARGET_OBJECTS:test_fs_obj>
  ${TEST_SSH_SOURCES}
  $<TARGET_OBJECTS:test_json_obj>
  $<TARGET_OBJECTS:test_daemon_obj>
  $<TARGET_OBJECTS:test_network_obj>
)

set(COMMON_TEST_OBJECTS
  ${MINIMAL_TEST_OBJECTS}
  ${TEST_PROCESS_SOURCES}
  $<TARGET_OBJECTS:test_fs_obj>
  ${TEST_SSH_SOURCES}
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
)

# Formatter test sources
set(FORMATTER_TEST_SOURCES
  test_format_utils.cpp
  test_output_formatter.cpp
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

# Main tests executable
add_executable(multipass_tests
  ${COMMON_TEST_OBJECTS}
  test_base_snapshot.cpp
  test_base_virtual_machine.cpp
  test_base_virtual_machine_factory.cpp
  test_basic_process.cpp
  test_cloud_init_iso.cpp
  test_common_callbacks.cpp
  test_constants.cpp
  test_delayed_shutdown.cpp
  test_disabled_copy_move.cpp
  test_id_mappings.cpp
  test_image_vault.cpp
  test_image_vault_utils.cpp
  test_json_utils.cpp
  test_mock_standard_paths.cpp
  test_mount_handler.cpp
  test_petname.cpp
  test_private_pass_provider.cpp
  test_qemuimg_process_spec.cpp
  test_simple_streams_index.cpp
  test_simple_streams_manifest.cpp
  test_singleton.cpp
  test_ssl_cert_provider.cpp
  test_timer.cpp
  test_top_catch_all.cpp
  test_utils.cpp
  test_yaml_node_utils.cpp
  test_vm_mount.cpp
  test_with_mocked_bin_path.cpp
  test_blueprint_provider.cpp
  test_file_ops.cpp
  test_recursive_dir_iter.cpp
  test_log.cpp
  test_exception.cpp
  test_permission_utils.cpp
  test_standard_logger.cpp
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
  poco_utils
)

target_link_libraries(multipass_daemon_tests ${DAEMON_TEST_LIBRARIES})
target_link_libraries(multipass_client_ssh_tests ${CLIENT_SSH_TEST_LIBRARIES})
target_link_libraries(multipass_memory_tests ${MEMORY_TEST_LIBRARIES})
target_link_libraries(multipass_formatter_tests ${FORMATTER_TEST_LIBRARIES})
target_link_libraries(multipass_network_tests ${NETWORK_TEST_LIBRARIES})
target_link_libraries(multipass_tests ${DAEMON_TEST_LIBRARIES} poco_utils)

# need https://github.com/google/googletest/commit/a09ea700d32bab83325aff9ff34d0582e50e3997 before this can be removed
if (CMAKE_CXX_COMPILER_ID STREQUAL "Clang")
  add_compile_options(-Wno-inconsistent-missing-override)
endif()

add_test(NAME multipass_daemon_tests COMMAND multipass_daemon_tests)
add_test(NAME multipass_client_ssh_tests COMMAND multipass_client_ssh_tests)
add_test(NAME multipass_memory_tests COMMAND multipass_memory_tests)
add_test(NAME multipass_formatter_tests COMMAND multipass_formatter_tests)
add_test(NAME multipass_network_tests COMMAND multipass_network_tests)
add_test(NAME multipass_tests COMMAND multipass_tests)

# Add backend-specific compile definitions
foreach(BACKEND IN LISTS MULTIPASS_BACKENDS)
  string(TOUPPER ${BACKEND}_ENABLED DEF)
  add_compile_definitions(${DEF})
endforeach()

# Set architecture compile definition
string(TOLOWER ${CMAKE_HOST_SYSTEM_PROCESSOR} MANIFEST_ARCH)
if (${MANIFEST_ARCH} STREQUAL "x86_64")
  string(TOLOWER "amd64" MANIFEST_ARCH)
endif()
add_compile_definitions(MANIFEST_ARCH="${MANIFEST_ARCH}")

file(GLOB_RECURSE MANIFEST_FILES RELATIVE "${CMAKE_CURRENT_SOURCE_DIR}" "test_data/*.in")

foreach (MANIFEST_IN IN LISTS MANIFEST_FILES)
  string(REGEX REPLACE "\.in$" "" MANIFEST_OUT ${MANIFEST_IN})
  configure_file(
    ${MANIFEST_IN}
    ${CMAKE_RUNTIME_OUTPUT_DIRECTORY}/${MANIFEST_OUT}
    @ONLY)
endforeach()

file(COPY test_data DESTINATION ${CMAKE_RUNTIME_OUTPUT_DIRECTORY} PATTERN "*\.in" EXCLUDE)

# Mock binaries for testing BasicProcess (cross-platform compatible)
add_executable(mock_process
  mock_process.cpp)

set_target_properties(mock_process
  PROPERTIES
  RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/bin/mocks"
  RUNTIME_OUTPUT_DIRECTORY_DEBUG "${CMAKE_BINARY_DIR}/bin/mocks"
  RUNTIME_OUTPUT_DIRECTORY_RELEASE "${CMAKE_BINARY_DIR}/bin/mocks")

add_dependencies(multipass_tests mock_process)
add_dependencies(multipass_daemon_tests mock_process)
add_dependencies(multipass_client_ssh_tests mock_process)
add_dependencies(multipass_memory_tests mock_process)
add_dependencies(multipass_formatter_tests mock_process)
add_dependencies(multipass_network_tests mock_process)



add_subdirectory(${MULTIPASS_PLATFORM})

foreach(BACKEND IN LISTS MULTIPASS_BACKENDS)
  add_subdirectory(${BACKEND})
endforeach()

if (UNIX)
  add_subdirectory(unix)
endif()
