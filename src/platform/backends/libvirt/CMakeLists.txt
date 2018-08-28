#Copyright © 2018 Canonical Ltd.
#
#This program is free software : you can redistribute it and / or modify
#it under the terms of the GNU General Public License version 3 as
#published by the Free Software Foundation.
#
#This program is distributed in the hope that it will be useful,
#but WITHOUT ANY WARRANTY; without even the implied warranty of
#MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.See the
#GNU General Public License for more details.
#
#You should have received a copy of the GNU General Public License
#along with this program.If not, see < http: // www.gnu.org/licenses/>.
#

find_package(PkgConfig)
pkg_check_modules(LIBVIRT libvirt REQUIRED)

add_library(libvirt_backend STATIC
  libvirt_virtual_machine_factory.cpp
  libvirt_virtual_machine.cpp)

target_include_directories(libvirt_backend PRIVATE ${LIBVIRT_INCLUDE_DIRS})
target_link_libraries(libvirt_backend
  backend_utils
  fmt
  ip_address
  Qt5::Core
  ssh
  utils
  ${LIBVIRT_LIBRARIES})
