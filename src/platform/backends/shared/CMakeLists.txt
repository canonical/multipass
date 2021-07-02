# Copyright © 2019-2021 Canonical Ltd.
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

add_library(shared STATIC
  base_virtual_machine.cpp
  base_virtual_machine_factory.cpp
  sshfs_server_process_spec.cpp
  ${CMAKE_SOURCE_DIR}/include/multipass/process/basic_process.h)

target_link_libraries(shared
  iso
  process
  utils
  Qt5::Core)

if(UNIX)
  add_subdirectory(qemu_img_utils)
endif()

if(LINUX)
  add_subdirectory(linux)
endif()
