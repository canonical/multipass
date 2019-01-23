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
add_library(backend_utils STATIC
  backend_utils.cpp
  process_spec.cpp
  process_factory.cpp
  sshfs_server_process_spec.cpp
  process.cpp)

INCLUDE_DIRECTORIES(backend_utils
  .)

target_link_libraries(backend_utils
  fmt
  utils
  Qt5::Core)

# Require AppArmor support on linux
include(FindPkgConfig)
pkg_search_module(APPARMOR libapparmor REQUIRED)
add_definitions(-DAPPARMOR_ENABLED)

target_sources(backend_utils PRIVATE
  apparmor.cpp
  apparmored_process_factory.cpp
)

target_link_libraries(backend_utils
  apparmor
)
