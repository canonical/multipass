# Copyright © 2017 Canonical Ltd.
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
# Authored by: Chris Townsend <christopher.townsend@canonical.com>

find_package(Qt5Core)

add_library(wraptext STATIC
            wrap_text.cpp)

target_include_directories(wraptext INTERFACE
  ${CMAKE_CURRENT_SOURCE_DIR})

target_link_libraries(wraptext
  Qt5::Core)
