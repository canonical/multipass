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

if(MSVC)
add_library(client_platform STATIC
  client_platform_windows.cpp)
elseif(APPLE)
add_library(client_platform STATIC
  client_platform.cpp)
else()
add_library(client_platform STATIC
  client_platform.cpp)
endif()

target_link_libraries(client_platform PUBLIC
  Qt6::Core
  console
  fmt::fmt-header-only)
