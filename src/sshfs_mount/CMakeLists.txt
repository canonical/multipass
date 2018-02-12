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
function(add_sshfs_mount_target TARGET_NAME)
  add_definitions(-DWITH_SERVER)

  add_library(${TARGET_NAME} STATIC
    sshfs_mount.cpp
    sftp_server.cpp
    ${CMAKE_SOURCE_DIR}/include/multipass/sshfs_mount/sshfs_mount.h)

  target_link_libraries(${TARGET_NAME}
    fmt
    ssh
    utils
    Qt5::Core)
endfunction()

add_sshfs_mount_target(sshfs_mount)
if(MULTIPASS_ENABLE_TESTS)
  add_sshfs_mount_target(sshfs_mount_test)
endif()
