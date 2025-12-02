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

add_library(logger STATIC
  log.cpp
  multiplexing_logger.cpp
  standard_logger.cpp)

target_link_libraries(logger
  rust_logger #This is because every staticlib contains the cxx bridge,
  # But the cxx bridge source cannot be isolated
  fmt::fmt-header-only
  Qt6::Core)
