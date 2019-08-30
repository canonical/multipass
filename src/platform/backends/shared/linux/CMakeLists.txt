# Copyright © 2018-2019 Canonical Ltd.
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

set (CMAKE_AUTOMOC ON)

include(FindPkgConfig)
pkg_search_module(APPARMOR libapparmor REQUIRED)

add_library(shared_linux STATIC
  apparmor.cpp
  backend_utils.cpp
  linux_process.cpp
  process_spec.cpp
  process_factory.cpp
  qemuimg_process_spec.cpp)

target_link_libraries(shared_linux
  apparmor
  fmt
  utils
  Qt5::Core)
