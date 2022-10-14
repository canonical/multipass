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

set(CMAKE_AUTOMOC ON)
function(add_sshfs_mount_target TARGET_NAME)
  add_definitions(-DWITH_SERVER)

  add_library(${TARGET_NAME} STATIC
    sshfs_mount.cpp
    sshfs_mount_handler.cpp
    sftp_server.cpp
    # Need to run MOC on these
    sshfs_mount.h
    ${CMAKE_SOURCE_DIR}/include/multipass/sshfs_mount/sshfs_mount_handler.h)

  target_link_libraries(${TARGET_NAME}
    fmt
    logger
    platform
    ssh
    utils
    Qt6::Core)
endfunction()

add_sshfs_mount_target(sshfs_mount)
if(MULTIPASS_ENABLE_TESTS)
  add_sshfs_mount_target(sshfs_mount_test)
endif()

add_executable(sshfs_server
  sshfs_server.cpp)

target_link_libraries(sshfs_server
  logger
  sshfs_mount)

target_include_directories(sshfs_server
  BEFORE
    PRIVATE ${CMAKE_SOURCE_DIR}/src
)

install(TARGETS sshfs_server
  DESTINATION bin
  COMPONENT multipassd)
