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

add_library(ui_wording STATIC
  launch_messages.cpp
)

target_link_libraries(ui_wording PUBLIC
  fmt::fmt-header-only
  rpc
)

target_include_directories(ui_wording
  PUBLIC "${CMAKE_SOURCE_DIR}/include"
)
