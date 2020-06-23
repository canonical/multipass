# Copyright © 2017-2020 Canonical Ltd.
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

add_library(hyperv_backend STATIC
  hyperv_virtual_machine.cpp
  hyperv_virtual_machine_factory.cpp
  powershell.cpp
  ${CMAKE_SOURCE_DIR}/include/multipass/process/basic_process.h
  ${CMAKE_SOURCE_DIR}/include/multipass/process/process.h)

target_link_libraries(hyperv_backend
  Qt5::Core
  fmt
  ip_address
  logger
  process
  ssh
  utils
  yaml)
