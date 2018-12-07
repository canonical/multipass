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

add_library(confinement STATIC
  confinement_system.cpp
  process.cpp
  process_spec.cpp
  unconfined_system.cpp
  # we need to run moc on this header
  ${CMAKE_SOURCE_DIR}/include/multipass/process.h
)

target_link_libraries(confinement
  fmt
  logger
  Qt5::Core
)

# Require AppArmor support included on Linux
if (${CMAKE_SYSTEM_NAME} MATCHES "Linux")
  include(FindPkgConfig)
  pkg_search_module(APPARMOR libapparmor REQUIRED)
  add_definitions(-DAPPARMOR_ENABLED)

  target_sources(confinement PRIVATE
    apparmor.cpp
    apparmor_confined_system.cpp
  )

  target_link_libraries(confinement
    apparmor
  )
endif()
