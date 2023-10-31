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

add_library(shared STATIC
  base_snapshot.cpp
  base_virtual_machine.cpp
  base_virtual_machine_factory.cpp
  sshfs_server_process_spec.cpp
  ${CMAKE_SOURCE_DIR}/include/multipass/process/basic_process.h)

target_link_libraries(shared
  iso
  process
  scope_guard
  utils
  Qt6::Core)

add_subdirectory(qemu_img_utils)
add_subdirectory(${MULTIPASS_PLATFORM})
