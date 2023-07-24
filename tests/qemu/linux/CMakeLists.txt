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

target_sources(multipass_tests
  PRIVATE
    ${CMAKE_CURRENT_LIST_DIR}/test_dnsmasq_server.cpp
    ${CMAKE_CURRENT_LIST_DIR}/test_dnsmasq_process_spec.cpp
    ${CMAKE_CURRENT_LIST_DIR}/test_firewall_config.cpp
    ${CMAKE_CURRENT_LIST_DIR}/test_qemu_platform_detail.cpp
)

target_include_directories(multipass_tests PRIVATE ${CMAKE_SOURCE_DIR}/src/platform/backends/qemu)

add_executable(dnsmasq
  mock_dnsmasq.cpp)

set_target_properties(dnsmasq
  PROPERTIES
  RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/bin/mocks"
  RUNTIME_OUTPUT_DIRECTORY_DEBUG "${CMAKE_BINARY_DIR}/bin/mocks"
  RUNTIME_OUTPUT_DIRECTORY_RELEASE "${CMAKE_BINARY_DIR}/bin/mocks")

target_link_libraries(dnsmasq
        Qt6::Core)

add_executable(dhcp_release
  mock_dhcp_release.cpp)

set_target_properties(dhcp_release
  PROPERTIES
  RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/bin/mocks"
  RUNTIME_OUTPUT_DIRECTORY_DEBUG "${CMAKE_BINARY_DIR}/bin/mocks"
  RUNTIME_OUTPUT_DIRECTORY_RELEASE "${CMAKE_BINARY_DIR}/bin/mocks")

add_dependencies(multipass_tests
  dnsmasq
  dhcp_release
)
