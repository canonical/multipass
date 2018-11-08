# Copyright © 2017-2018 Canonical Ltd.
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
# Authored by: Alberto Aguirre <alberto.aguirre@canonical.com>

set(CMAKE_AUTOMOC ON)

include(FindPkgConfig)

pkg_search_module(APPARMOR apparmor)

add_library(qemu_backend STATIC
  apparmor.cpp
  apparmor_confined_system.cpp
  dhcp_release_process_spec.cpp
  dnsmasq_process_spec.cpp
  dnsmasq_server.cpp
  qemu_process_spec.cpp
  qemu_virtual_machine_factory.cpp
  qemu_virtual_machine.cpp
  process.cpp
  process_spec.cpp
  unconfined_system.cpp
)

target_link_libraries(qemu_backend
  apparmor
  backend_utils
  fmt
  logger
  Qt5::Core
  Qt5::Network
  ip_address
  ssh
  utils
  yaml)

install(PROGRAMS check_kvm_support
  DESTINATION bin)
