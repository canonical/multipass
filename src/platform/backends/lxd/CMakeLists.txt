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
#

add_library(lxd_backend STATIC
  lxd_mount_handler.cpp
  lxd_request.cpp
  lxd_virtual_machine.cpp
  lxd_virtual_machine_factory.cpp
  lxd_vm_image_vault.cpp)

target_link_libraries(lxd_backend
  Qt6::Core
  Qt6::Network
  fmt
  ip_address
  logger
  network
  qemu_img_utils
  rpc
  ssh
  utils
  yaml)
