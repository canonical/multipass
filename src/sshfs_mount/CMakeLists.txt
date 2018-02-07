# Copyright © 2017 Canonical Ltd.
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

set(CMAKE_AUTOMOC ON)

add_definitions(-DWITH_SERVER)

set_source_files_properties(sshfs_mount.cpp PROPERTIES COMPILE_FLAGS -Wno-pedantic)
set_source_files_properties(sftp_server.cpp PROPERTIES COMPILE_FLAGS -Wno-pedantic)

add_library(sshfs_mount STATIC
  sshfs_mount.cpp
  sftp_server.cpp
  ${CMAKE_SOURCE_DIR}/include/multipass/sshfs_mount/sshfs_mount.h)

target_link_libraries(sshfs_mount
  fmt
  ssh
  utils
  Qt5::Core)
