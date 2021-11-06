# Copyright © 2019-2021 Canonical Ltd.
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
set(CMAKE_AUTORCC ON)

add_executable(multipass_gui
  argparser.cpp
  client_gui.cpp
  main.cpp
  gui_cmd.cpp
  resources/resources.qrc)

set_target_properties(multipass_gui
  PROPERTIES
  OUTPUT_NAME multipass.gui)

target_link_libraries(multipass_gui
  client_common
  client_platform
  fmt
  formatter
  rpc
  settings
  qhotkey
  Qt5::Core
  Qt5::Network
  Qt5::Widgets)

install(TARGETS multipass_gui
  DESTINATION bin
  COMPONENT multipass_gui)
