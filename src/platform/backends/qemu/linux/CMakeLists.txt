# Copyright © 2021 Canonical Ltd.
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

add_library(qemu_platform_detail
  qemu_platform_detail_linux.cpp)

target_include_directories(qemu_platform_detail PRIVATE ../)

target_link_libraries(qemu_platform_detail
  #shared_linux
  logger
  utils
  Qt5::Core)
