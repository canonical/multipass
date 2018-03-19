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
#
# Authored by: Alberto Aguirre <alberto.aguirre@canonical.com>

set(CMAKE_AUTOMOC ON)

add_library(daemon STATIC
  daemon.cpp
  daemon_config.cpp
  daemon_rpc.cpp
  default_vm_image_vault.cpp
  json_writer.cpp
  ubuntu_image_host.cpp)

target_link_libraries(daemon
  fmt
  iso
  logger
  rpc
  petname
  platform
  simplestreams
  ssh
  sshfs_mount
  utils
  Qt5::Core
  Qt5::Network
  yaml)

add_executable(multipassd daemon_main.cpp)

target_link_libraries(multipassd
  fmt
  daemon)

install(TARGETS multipassd
  DESTINATION bin
  COMPONENT multipassd)
