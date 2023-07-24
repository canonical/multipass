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

set(CMAKE_AUTOMOC ON)

add_library(process STATIC
  basic_process.cpp
  process_spec.cpp
  qemuimg_process_spec.cpp
  simple_process_spec.cpp
  ${CMAKE_SOURCE_DIR}/include/multipass/process/basic_process.h
  ${CMAKE_SOURCE_DIR}/include/multipass/process/process.h)

target_link_libraries(process
  logger
  Qt6::Core)
