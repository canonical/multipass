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

set (CMAKE_AUTOMOC ON)

add_definitions(-DHOST_ARCH="${HOST_ARCH}")

add_library(qemu_backend STATIC
  qemu_base_process_spec.cpp
  qemu_mount_handler.cpp
  qemu_snapshot.cpp
  qemu_vm_process_spec.cpp
  qemu_vmstate_process_spec.cpp
  qemu_virtual_machine_factory.cpp
  qemu_virtual_machine.cpp)

target_link_libraries(qemu_backend
  fmt
  ip_address
  logger
  qemu_img_utils
  qemu_platform_detail
  scope_guard
  utils
  Qt6::Core)

add_subdirectory(${MULTIPASS_PLATFORM})
