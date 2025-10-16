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

enable_language(OBJCXX)

add_library(apple_backend STATIC
  apple_virtual_machine.cpp
  apple_virtual_machine_factory.cpp
  apple_vz_bridge.mm
  apple_vz_wrapper.cpp
)

set_source_files_properties(apple_vz_bridge.mm PROPERTIES
  LANGUAGE OBJCXX
  COMPILE_FLAGS "-fobjc-arc"
)

find_library(FOUNDATION_FRAMEWORK Foundation REQUIRED)
find_library(VIRTUALIZATION_FRAMEWORK Virtualization REQUIRED)

target_link_libraries(apple_backend
  PUBLIC
    fmt::fmt-header-only
  PRIVATE
    daemon
    ${FOUNDATION_FRAMEWORK}
    ${VIRTUALIZATION_FRAMEWORK}
    Qt6::Core)
