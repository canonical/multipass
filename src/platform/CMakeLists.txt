# Copyright © 2017-2018 Canonical Ltd.
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
# Authored by: Alberto Aguirre <alberto.aguirre@canonical.com>

if(MSVC)
  add_library(platform STATIC platform_win.cpp)
  target_link_libraries(platform hyperv_backend)
elseif(APPLE)
  add_library(platform STATIC platform_osx.cpp)
  target_link_libraries(platform hyperkit_backend)
else()
  add_library(platform STATIC platform.cpp)
  target_link_libraries(platform qemu_backend)
endif()

add_subdirectory(backends)
add_subdirectory(console)
add_subdirectory(client)
