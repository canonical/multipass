# Copyright © 2017-2019 Canonical Ltd.
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

set (CMAKE_AUTOMOC ON)

add_library(qemu_backend STATIC
  dnsmasq_process_spec.cpp
  dnsmasq_server.cpp
  qemu_vm_process_spec.cpp
  qemu_virtual_machine_factory.cpp
  qemu_virtual_machine.cpp
  ${CMAKE_SOURCE_DIR}/include/multipass/process.h)

target_link_libraries(qemu_backend
  fmt
  logger
  Qt5::Core
  Qt5::Network
  ip_address
  shared_linux
  ssh
  utils
  yaml)

install(PROGRAMS check_kvm_support
  DESTINATION bin)
