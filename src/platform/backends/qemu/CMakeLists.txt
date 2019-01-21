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

include_directories(qemu_backend
  ../backend_utils)

add_library(qemu_backend STATIC
  dnsmasq_process_spec.cpp
  dnsmasq_server.cpp
  qemuimg_process_spec.cpp
  qemu_process_spec.cpp
  qemu_virtual_machine_factory.cpp
  qemu_virtual_machine.cpp)

target_link_libraries(qemu_backend
  backend_utils
  fmt
  logger
  process
  Qt5::Core
  Qt5::Network
  ip_address
  ssh
  utils
  yaml)

install(PROGRAMS check_kvm_support
  DESTINATION bin)
