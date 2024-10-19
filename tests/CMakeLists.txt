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

add_executable(multipass_tests
  blueprint_test_lambdas.cpp
  common.cpp
  daemon_test_fixture.cpp
  file_operations.cpp
  image_host_remote_count.cpp
  json_test_utils.cpp
  main.cpp
  mischievous_url_downloader.cpp
  mock_logger.cpp
  mock_openssl_syscalls.cpp
  mock_process_factory.cpp
  mock_sftp.cpp
  mock_sftpserver.cpp
  mock_ssh.cpp
  mock_ssh_client.cpp
  mock_standard_paths.cpp
  path.cpp
  reset_process_factory.cpp
  stub_process_factory.cpp
  temp_dir.cpp
  temp_file.cpp
  test_alias_dict.cpp
  test_argparser.cpp
  test_base_snapshot.cpp
  test_base_virtual_machine.cpp
  test_base_virtual_machine_factory.cpp
  test_basic_process.cpp
  test_cli_client.cpp
  test_cli_prompters.cpp
  test_client_cert_store.cpp
  test_client_common.cpp
  test_cloud_init_iso.cpp
  test_common_callbacks.cpp
  test_constants.cpp
  test_custom_image_host.cpp
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
  test_delayed_shutdown.cpp
  test_disabled_copy_move.cpp
  test_format_utils.cpp
  test_global_settings_handlers.cpp
  test_id_mappings.cpp
  test_image_vault.cpp
  test_instance_settings_handler.cpp
  test_ip_address.cpp
  test_json_utils.cpp
  test_memory_size.cpp
  test_mock_standard_paths.cpp
  test_mount_handler.cpp
  test_new_release_monitor.cpp
  test_output_formatter.cpp
  test_persistent_settings_handler.cpp
  test_petname.cpp
  test_private_pass_provider.cpp
  test_qemuimg_process_spec.cpp
  test_remote_settings_handler.cpp
  test_setting_specs.cpp
  test_settings.cpp
  test_sftp_client.cpp
  test_sftpserver.cpp
  test_simple_streams_index.cpp
  test_simple_streams_manifest.cpp
  test_singleton.cpp
  test_ssh_client.cpp
  test_ssh_exec_failure.cpp
  test_ssh_key_provider.cpp
  test_ssh_process.cpp
  test_ssh_session.cpp
  test_sshfs_server_process_spec.cpp
  test_sshfsmount.cpp
  test_sshfs_mount_handler.cpp
  test_ssl_cert_provider.cpp
  test_timer.cpp
  test_top_catch_all.cpp
  test_ubuntu_image_host.cpp
  test_url_downloader.cpp
  test_utils.cpp
  test_yaml_node_utils.cpp
  test_vm_mount.cpp
  test_with_mocked_bin_path.cpp
  test_blueprint_provider.cpp
  test_sftp_dir_iterator.cpp
  test_sftp_utils.cpp
  test_file_ops.cpp
  test_recursive_dir_iter.cpp
)

target_include_directories(multipass_tests
  PRIVATE ${CMAKE_SOURCE_DIR}
  PRIVATE ${CMAKE_SOURCE_DIR}/src
  PRIVATE ${CMAKE_SOURCE_DIR}/src/platform/backends
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

target_link_libraries(multipass_tests
  cert
  client
  daemon
  delayed_shutdown
  gmock_main
  gtest_main
  ip_address
  iso
  network
  petname
  poco_utils
  settings
  simplestreams
  sftp_test
  ssh_test
  ssh_client_test
  sshfs_mount_test
  utils_test
  # 3rd-party
  premock
  scope_guard
  yaml
)

# need https://github.com/google/googletest/commit/a09ea700d32bab83325aff9ff34d0582e50e3997 before this can be removed
if (CMAKE_CXX_COMPILER_ID STREQUAL "Clang")
  target_compile_options(multipass_tests PRIVATE -Wno-inconsistent-missing-override)
endif()

add_test(NAME multipass_tests
  COMMAND multipass_tests
)

foreach(BACKEND IN LISTS MULTIPASS_BACKENDS)
  string(TOUPPER ${BACKEND}_ENABLED DEF)
  target_compile_definitions(multipass_tests PRIVATE ${DEF})
endforeach()

string(TOLOWER ${CMAKE_HOST_SYSTEM_PROCESSOR} MANIFEST_ARCH)
if (${MANIFEST_ARCH} STREQUAL "x86_64")
  string(TOLOWER "amd64" MANIFEST_ARCH)
endif()

target_compile_definitions(multipass_tests PRIVATE -DMANIFEST_ARCH="${MANIFEST_ARCH}")

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

target_include_directories(multipass_tests
  BEFORE
    PRIVATE ${CMAKE_SOURCE_DIR}/src/platform/backends/shared/${MULTIPASS_PLATFORM}
)

add_subdirectory(${MULTIPASS_PLATFORM})

foreach(BACKEND IN LISTS MULTIPASS_BACKENDS)
  add_subdirectory(${BACKEND})
endforeach()

if (UNIX)
  add_subdirectory(unix)
endif()
