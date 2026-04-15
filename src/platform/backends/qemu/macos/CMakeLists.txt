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

add_library(qemu_platform_impl
  qemu_platform_macos.cpp)

target_include_directories(qemu_platform_impl PRIVATE ../)

target_link_libraries(qemu_platform_impl
  fmt::fmt-header-only
  logger
  utils
  Qt6::Core)
