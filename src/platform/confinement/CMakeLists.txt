# Copyright © 2018 Canonical Ltd.
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

include(FindPkgConfig)

pkg_search_module(APPARMOR apparmor)

add_library(confinement STATIC
  apparmor.cpp
  apparmor_confined_system.cpp
  confinement_system.cpp
  process.cpp
  process_spec.cpp
  unconfined_system.cpp
  # we need to run moc on this header
  ${CMAKE_SOURCE_DIR}/include/multipass/process.h
)

target_link_libraries(confinement
  apparmor
  fmt
  logger
  Qt5::Core
)
