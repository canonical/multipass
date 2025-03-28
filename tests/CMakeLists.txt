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

# Minimal sources needed by simplest tests
set(MINIMAL_TEST_SOURCES
  common.cpp
  main.cpp
  mock_logger.cpp
  mock_standard_paths.cpp
  temp_dir.cpp
  temp_file.cpp
)

# Network test specific common sources
set(NETWORK_COMMON_SOURCES
  ${MINIMAL_TEST_SOURCES}
  mischievous_url_downloader.cpp
  path.cpp # For test_data_path()
  image_host_remote_count.cpp # For count_remotes()
)

# Common sources for SSH and client tests
set(SSH_CLIENT_COMMON_SOURCES
  ${MINIMAL_TEST_SOURCES}
  mock_ssh.cpp
  mock_ssh_client.cpp
  mock_sftp.cpp
  mock_sftpserver.cpp
  mock_process_factory.cpp
  mock_openssl_syscalls.cpp # For OpenSSL mocks including ut_EVP_PBE_scrypt
  stub_process_factory.cpp
  reset_process_factory.cpp
  daemon_test_fixture.cpp # For daemon mocking
  file_operations.cpp # For file operations in SSH tests
  path.cpp # For path utilities
  json_test_utils.cpp # For JSON handling in client and SSH tests
)

# Common sources for daemon tests
set(DAEMON_COMMON_SOURCES
  ${MINIMAL_TEST_SOURCES}
  mock_ssh.cpp # For SSH operations in daemon
  mock_ssh_client.cpp
  mock_sftp.cpp
  mock_sftpserver.cpp
  mock_process_factory.cpp
  mock_openssl_syscalls.cpp # For SSL operations
  stub_process_factory.cpp
  reset_process_factory.cpp
  daemon_test_fixture.cpp # Core daemon testing utilities
  file_operations.cpp # For filesystem operations
  path.cpp # For path utilities
  json_test_utils.cpp # For JSON operations
  blueprint_test_lambdas.cpp # For VM blueprint handling
  image_host_remote_count.cpp # For image operations
  mischievous_url_downloader.cpp # For network testing
)

# Common sources for QEMU tests
set(QEMU_COMMON_SOURCES
  ${MINIMAL_TEST_SOURCES}
  mock_process_factory.cpp # For process control
  mock_ssh.cpp # For VM SSH access
  mock_ssh_client.cpp
  mock_sftp.cpp
  mock_sftpserver.cpp
  mock_openssl_syscalls.cpp # For OpenSSL mocks including ut_EVP_PBE_scrypt
  stub_process_factory.cpp
  reset_process_factory.cpp
  daemon_test_fixture.cpp # For VM management
  file_operations.cpp # For filesystem operations
  path.cpp # For path utilities
  json_test_utils.cpp # For configuration
  blueprint_test_lambdas.cpp # For VM blueprints
  image_host_remote_count.cpp # For image management
)

# Common test sources for miscellaneous tests not requiring special mocks
set(COMMON_TEST_SOURCES
  ${MINIMAL_TEST_SOURCES}
  mock_process_factory.cpp # For basic process mocking
  stub_process_factory.cpp
  reset_process_factory.cpp
  file_operations.cpp # For filesystem operations
  path.cpp # For path utilities
  mock_openssl_syscalls.cpp # For OpenSSL mocks
  json_test_utils.cpp # For JSON parsing
  daemon_test_fixture.cpp # For daemon-related tests
  blueprint_test_lambdas.cpp # For blueprint tests
  image_host_remote_count.cpp # For image host tests
  mock_ssh.cpp # For SSH mocking in daemon and VM tests
  mock_ssh_client.cpp
  mock_sftp.cpp # For file operations over SSH
  mock_sftpserver.cpp
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
  ${DAEMON_COMMON_SOURCES}
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
  ${SSH_CLIENT_COMMON_SOURCES}
  ${SSH_TEST_SOURCES}
  ${CLIENT_TEST_SOURCES}
)

# Memory test executable
add_executable(multipass_memory_tests
  ${MINIMAL_TEST_SOURCES}
  ${MEMORY_TEST_SOURCES}
)

# Formatter test executable
add_executable(multipass_formatter_tests
  ${MINIMAL_TEST_SOURCES}
  ${FORMATTER_TEST_SOURCES}
)

# Network test executable
add_executable(multipass_network_tests
  ${NETWORK_COMMON_SOURCES}
  ${NETWORK_TEST_SOURCES}
)

# QEMU test executable
add_executable(multipass_qemu_tests
  ${QEMU_COMMON_SOURCES}
  ${QEMU_TEST_SOURCES}
  test_with_mocked_bin_path.cpp
)

# Main tests executable
add_executable(multipass_tests
  ${COMMON_TEST_SOURCES}
  test_argparser.cpp
  test_base_snapshot.cpp
  test_base_virtual_machine.cpp
  test_base_virtual_machine_factory.cpp
  test_basic_process.cpp
  test_cloud_init_iso.cpp
  test_common_callbacks.cpp
  test_constants.cpp
  test_delayed_shutdown.cpp
  test_disabled_copy_move.cpp
  test_global_settings_handlers.cpp
  test_id_mappings.cpp
  test_image_vault.cpp
  test_image_vault_utils.cpp
  test_instance_settings_handler.cpp
  test_json_utils.cpp
  test_mock_standard_paths.cpp
  test_mount_handler.cpp
  test_persistent_settings_handler.cpp
  test_petname.cpp
  test_private_pass_provider.cpp
  test_qemuimg_process_spec.cpp
  test_remote_settings_handler.cpp
  test_setting_specs.cpp
  test_settings.cpp
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
  test_standard_logger.cpp
)

# Common include directories
set(COMMON_INCLUDE_DIRS
  ${CMAKE_SOURCE_DIR}
  ${CMAKE_SOURCE_DIR}/src
  ${CMAKE_SOURCE_DIR}/src/platform/backends
)

target_include_directories(multipass_daemon_tests PRIVATE ${COMMON_INCLUDE_DIRS})
target_include_directories(multipass_client_ssh_tests PRIVATE ${COMMON_INCLUDE_DIRS})
target_include_directories(multipass_memory_tests PRIVATE ${COMMON_INCLUDE_DIRS})
target_include_directories(multipass_formatter_tests PRIVATE ${COMMON_INCLUDE_DIRS})
target_include_directories(multipass_network_tests PRIVATE ${COMMON_INCLUDE_DIRS})
target_include_directories(multipass_qemu_tests PRIVATE ${COMMON_INCLUDE_DIRS}
  ${CMAKE_SOURCE_DIR}/src/platform/backends/qemu
  ${CMAKE_SOURCE_DIR}/src/platform/backends/qemu/linux
)
target_include_directories(multipass_tests PRIVATE ${COMMON_INCLUDE_DIRS})

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

# Common libraries needed by all tests
set(COMMON_TEST_LIBRARIES
  client # For shared functionality
  gmock_main # For mocking
  gtest_main # For testing framework
  premock # For mocking
  scope_guard # For cleanup
  utils_test # For utility functions
)

# Common libraries for miscellaneous tests
set(COMMON_LIBRARIES
  ${COMMON_TEST_LIBRARIES}
  daemon # For daemon functionality
  ip_address # For networking
  network # For core network operations
  settings # For configuration management
  ssh_test # For SSH operations
  yaml # For configuration parsing
)

# Minimal libraries for memory tests
set(MEMORY_TEST_LIBRARIES
  ${COMMON_TEST_LIBRARIES}
  ip_address
  network
)

# Libraries for formatter tests
set(FORMATTER_TEST_LIBRARIES
  ${COMMON_TEST_LIBRARIES}
  settings # For settings management
  yaml # For YAML formatting
)

# Libraries for network tests
set(NETWORK_TEST_LIBRARIES
  ${COMMON_TEST_LIBRARIES}
  ip_address # For IP validation
  network # Core network functionality
  settings # For configuration
)

# Libraries for daemon tests
set(DAEMON_TEST_LIBRARIES
  ${COMMON_TEST_LIBRARIES}
  daemon # Core daemon functionality
  ip_address # For networking
  network # Network functionality 
  settings # Configuration management
  ssh_test # For SSH operations
  yaml # For config parsing
)

# Libraries for client and SSH tests
set(CLIENT_SSH_TEST_LIBRARIES
  ${COMMON_TEST_LIBRARIES}
  daemon # For daemon mock
  ip_address # For networking
  network # Network functionality
  settings # For settings/config
  sftp_test # For SFTP tests
  ssh_test # For SSH tests
  ssh_client_test # For SSH client tests
  sshfs_mount_test # For SFTP server tests
  yaml # For YAML parsing
)

# Libraries needed by QEMU tests
set(QEMU_TEST_LIBRARIES
  ${COMMON_TEST_LIBRARIES}
  daemon # For VM management
  ip_address # For networking
  network # For network operations
  settings # For VM configuration
  ssh_test # For SSH operations
  yaml # For configuration parsing
)

target_link_libraries(multipass_daemon_tests ${DAEMON_TEST_LIBRARIES})
target_link_libraries(multipass_client_ssh_tests ${CLIENT_SSH_TEST_LIBRARIES})
target_link_libraries(multipass_memory_tests ${MEMORY_TEST_LIBRARIES}) 
target_link_libraries(multipass_formatter_tests ${FORMATTER_TEST_LIBRARIES})
target_link_libraries(multipass_network_tests ${NETWORK_TEST_LIBRARIES})
target_link_libraries(multipass_qemu_tests ${COMMON_LIBRARIES})
target_link_libraries(multipass_tests ${COMMON_LIBRARIES})

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
