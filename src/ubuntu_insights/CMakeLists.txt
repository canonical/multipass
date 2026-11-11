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

if(NOT MULTIPASS_ENABLE_INSIGHTS)
  return()
endif()

add_library(ubuntu_insights STATIC libinsights_wrapper.cpp)

find_package(ubuntu-insights CONFIG REQUIRED)

target_link_libraries(ubuntu_insights PUBLIC ubuntu-insights::insights)
target_include_directories(ubuntu_insights PUBLIC "${CMAKE_SOURCE_DIR}/include")
