# Copyright © 2017-2019 Canonical Ltd.
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

set(CMAKE_AUTORCC ON)

add_library(client STATIC
  argparser.cpp
  client.cpp)

target_link_libraries(client
  cert
  commands
  fmt
  formatter
  platform
  rpc
  Qt5::Core)

add_executable(multipass
  main.cpp
  resources/resources.qrc)

target_link_libraries(multipass
  client)

install(TARGETS multipass
  DESTINATION bin
  COMPONENT multipass)

add_subdirectory(cmd)
add_subdirectory(formatter)
