# Copyright © 2017-2019 Canonical Ltd.
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
  animated_spinner.cpp
  common_cli.cpp
  launch.cpp
  purge.cpp
  exec.cpp
  find.cpp
  get.cpp
  help.cpp
  info.cpp
  list.cpp
  mount.cpp
  recover.cpp
  shell.cpp
  start.cpp
  stop.cpp
  suspend.cpp
  restart.cpp
  delete.cpp
  transfer.cpp
  umount.cpp
  version.cpp

  ${PLATFORM_COMMANDS})

target_link_libraries(commands
  client_common
  client_platform
  scp_client
  ssh_client
  rpc
  Qt5::Core
  Qt5::Network
  yaml)
