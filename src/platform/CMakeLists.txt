# Copyright © 2017-2021 Canonical Ltd.
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

if(LINUX)
  add_library(platform STATIC
    platform_linux.cpp
    platform_shared.cpp
    platform_unix.cpp)

  target_link_libraries(platform
    journaldlogger)
elseif(MSVC)
  add_library(platform STATIC
    platform_proprietary.cpp
    platform_shared.cpp
    platform_win.cpp)
  target_link_libraries(platform
    jsoncpp_static
    shared_win
    hyperv_backend
    scope_guard
    virtualbox_backend
    wineventlogger)
elseif(APPLE)
  add_library(platform STATIC
    platform_osx.cpp
    platform_proprietary.cpp
    platform_shared.cpp
    platform_unix.cpp)
  target_link_libraries(platform
    shared_macos
    hyperkit_backend
    virtualbox_backend
    Qt5::Core)
endif()

foreach(BACKEND IN LISTS MULTIPASS_BACKENDS)
  target_link_libraries(platform
    ${BACKEND}_backend)
endforeach()

include_directories(platform
  backends
  ${CMAKE_SOURCE_DIR}/src)

target_link_libraries(platform
  daemon
  shared
  update)

add_subdirectory(backends)
add_subdirectory(client)
add_subdirectory(console)
add_subdirectory(logger)
add_subdirectory(update)
