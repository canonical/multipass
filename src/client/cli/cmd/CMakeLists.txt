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

add_library(commands STATIC
  alias.cpp
  alias.h
  aliases.cpp
  aliases.h
  animated_spinner.cpp
  animated_spinner.h
  authenticate.cpp
  authenticate.h
  clone.cpp
  clone.h
  common_cli.cpp
  common_cli.h
  create_alias.cpp
  create_alias.h
  delete.cpp
  delete.h
  disable_zones.cpp
  disable_zones.h
  enable_zones.cpp
  enable_zones.h
  exec.cpp
  exec.h
  images.cpp
  images.h
  get.cpp
  get.h
  help.cpp
  help.h
  info.cpp
  info.h
  launch.cpp
  launch.h
  list.cpp
  list.h
  mount.cpp
  mount.h
  networks.cpp
  networks.h
  prefer.cpp
  prefer.h
  purge.cpp
  purge.h
  recover.cpp
  recover.h
  remote_settings_handler.cpp
  remote_settings_handler.h
  restart.cpp
  restart.h
  restore.cpp
  restore.h
  set.cpp
  set.h
  shell.cpp
  shell.h
  snapshot.cpp
  snapshot.h
  start.cpp
  start.h
  stop.cpp
  stop.h
  suspend.cpp
  suspend.h
  transfer.cpp
  transfer.h
  umount.cpp
  umount.h
  unalias.cpp
  unalias.h
  version.cpp
  version.h
  wait_ready.cpp
  wait_ready.h
  zones.cpp
  zones.h)

target_link_libraries(commands
  client_common
  client_platform
  sftp_client
  ssh_client
  rpc
  settings
  Qt6::Core
  Qt6::Network
  utils
  yaml-cpp::yaml-cpp)
