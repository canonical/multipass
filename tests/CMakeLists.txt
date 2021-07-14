# Copyright © 2017-2021 Canonical Ltd.
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

set(MULTIPASS_GMOCK_DIR ${CMAKE_SOURCE_DIR}/3rd-party/grpc/third_party/googletest/googlemock)
set(MULTIPASS_GTEST_DIR ${CMAKE_SOURCE_DIR}/3rd-party/grpc/third_party/googletest/googletest)

add_executable(multipass_tests
  common.cpp
  file_operations.cpp
  image_host_remote_count.cpp
  main.cpp
  mischievous_url_downloader.cpp
  mock_logger.cpp
  mock_process_factory.cpp
  mock_settings.cpp
  mock_standard_paths.cpp
  mock_sftp.cpp
  mock_sftpserver.cpp
  mock_ssh.cpp
  mock_ssh_client.cpp
  path.cpp
  reset_process_factory.cpp
  stub_process_factory.cpp
  temp_dir.cpp
  temp_file.cpp
  test_argparser.cpp
  test_base_virtual_machine.cpp
  test_base_virtual_machine_factory.cpp
  test_basic_process.cpp
  test_cli_client.cpp
  test_client_cert_store.cpp
  test_cloud_init_iso.cpp
  test_constants.cpp
  test_custom_image_host.cpp
  test_daemon.cpp
  test_daemon_find.cpp
  test_delayed_shutdown.cpp
  test_format_utils.cpp
  test_output_formatter.cpp
  test_image_vault.cpp
  test_ip_address.cpp
  test_memory_size.cpp
  test_metrics_provider.cpp
  test_new_release_monitor.cpp
  test_petname.cpp
  test_platform_shared.cpp
  test_private_pass_provider.cpp
  test_mock_settings.cpp
  test_mock_standard_paths.cpp
  test_qemuimg_process_spec.cpp
  test_simple_streams_index.cpp
  test_simple_streams_manifest.cpp
  test_singleton.cpp
  test_sftp_client.cpp
  test_sftpserver.cpp
  test_ssl_cert_provider.cpp
  test_sshfs_server_process_spec.cpp
  test_sshfsmount.cpp
  test_sshfsmounts.cpp
  test_ssh_client.cpp
  test_ssh_key_provider.cpp
  test_ssh_process.cpp
  test_ssh_session.cpp
  test_timer.cpp
  test_top_catch_all.cpp
  test_ubuntu_image_host.cpp
  test_url_downloader.cpp
  test_utils.cpp
  test_with_mocked_bin_path.cpp
  test_workflow_provider.cpp

  ${MULTIPASS_GMOCK_DIR}/src/gmock-all.cc
  ${MULTIPASS_GTEST_DIR}/src/gtest-all.cc
)

target_include_directories(multipass_tests
  PRIVATE ${CMAKE_SOURCE_DIR}
  PRIVATE ${CMAKE_SOURCE_DIR}/src
  PRIVATE ${CMAKE_SOURCE_DIR}/src/platform/backends
  PRIVATE ${MULTIPASS_GTEST_DIR}
  PRIVATE ${MULTIPASS_GTEST_DIR}/include
  PRIVATE ${MULTIPASS_GMOCK_DIR}
  PRIVATE ${MULTIPASS_GMOCK_DIR}/include
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

target_link_libraries(multipass_tests
  cert
  client
  daemon
  delayed_shutdown
  ip_address
  iso
  metrics
  network
  petname
  poco_utils
  simplestreams
  sftp_test
  ssh_test
  ssh_client_test
  sshfs_mount_test
  utils
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

file(COPY test_data DESTINATION ${CMAKE_RUNTIME_OUTPUT_DIRECTORY})

# Mock binaries for testing BasicProcess (cross-platform compatible)
add_executable(mock_process
  mock_process.cpp)

set_target_properties(mock_process
  PROPERTIES
  RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/bin/mocks"
  RUNTIME_OUTPUT_DIRECTORY_DEBUG "${CMAKE_BINARY_DIR}/bin/mocks"
  RUNTIME_OUTPUT_DIRECTORY_RELEASE "${CMAKE_BINARY_DIR}/bin/mocks")

add_dependencies(multipass_tests mock_process)

if(LINUX OR ENABLE_QEMU)
  add_subdirectory(qemu)
endif()

if(LINUX)
  target_include_directories(multipass_tests
    BEFORE
      PRIVATE ${CMAKE_SOURCE_DIR}/src/platform/backends/shared/linux
  )

  add_subdirectory(libvirt)
  add_subdirectory(linux)
  add_subdirectory(lxd)
endif()
