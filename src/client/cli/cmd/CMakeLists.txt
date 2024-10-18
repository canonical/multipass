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
  aliases.cpp
  animated_spinner.cpp
  authenticate.cpp
  clone.cpp
  common_cli.cpp
  create_alias.cpp
  delete.cpp
  exec.cpp
  find.cpp
  get.cpp
  help.cpp
  info.cpp
  launch.cpp
  list.cpp
  mount.cpp
  networks.cpp
  prefer.cpp
  purge.cpp
  recover.cpp
  remote_settings_handler.cpp
  restart.cpp
  restore.cpp
  set.cpp
  shell.cpp
  snapshot.cpp
  start.cpp
  stop.cpp
  suspend.cpp
  transfer.cpp
  umount.cpp
  unalias.cpp
  version.cpp
)

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
  yaml)
