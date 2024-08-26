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

set(BRIDGE_HELPER_EXEC_NAME "bridge_helper")

add_library(qemu_platform_detail
  dnsmasq_process_spec.cpp
  dnsmasq_server.cpp
  firewall_config.cpp
  qemu_platform_detail_linux.cpp)

target_compile_definitions(qemu_platform_detail PRIVATE BRIDGE_HELPER_EXEC_NAME_CPP="${BRIDGE_HELPER_EXEC_NAME}")

target_include_directories(qemu_platform_detail PRIVATE ../)

target_link_libraries(qemu_platform_detail
  logger
  semver
  shared_linux
  utils
  Qt6::Core)

add_executable(${BRIDGE_HELPER_EXEC_NAME} bridge_helper.c)

install(TARGETS ${BRIDGE_HELPER_EXEC_NAME}
  DESTINATION bin
  COMPONENT bridge_helper)
