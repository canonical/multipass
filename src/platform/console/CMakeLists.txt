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

if(MSVC)
add_library(console STATIC
    console.cpp
    windows_console.cpp)
else()
add_library(console STATIC
    console.cpp
    terminal.cpp
    unix_console.cpp
    unix_terminal.cpp)
endif()

target_link_libraries(console
  client_platform
  libssh)
