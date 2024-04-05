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

add_library(daemon STATIC
  cli.cpp
  common_image_host.cpp
  custom_image_host.cpp
  daemon.cpp
  daemon_config.cpp
  daemon_init_settings.cpp
  daemon_rpc.cpp
  default_vm_image_vault.cpp
  instance_settings_handler.cpp
  runtime_instance_info_helper.cpp
  snapshot_settings_handler.cpp
  ubuntu_image_host.cpp)

include_directories(daemon
  ${CMAKE_SOURCE_DIR}/src/platform/backends)

target_link_libraries(daemon
  cert
  delayed_shutdown
  fmt
  logger
  petname
  platform
  rpc
  settings
  simplestreams
  ssh
  sshfs_mount
  utils
  Qt6::Core
  Qt6::Network
  blueprint_provider
  xz_image_decoder
  yaml)

add_library(delayed_shutdown STATIC
  delayed_shutdown_timer.cpp
  ${CMAKE_SOURCE_DIR}/include/multipass/delayed_shutdown_timer.h)

target_link_libraries(delayed_shutdown
  fmt
  logger
  ssh_common
  Qt6::Core
)

add_executable(multipassd daemon_main.cpp)

target_link_libraries(multipassd
  fmt
  daemon)

install(TARGETS multipassd
  DESTINATION bin
  COMPONENT multipassd)
