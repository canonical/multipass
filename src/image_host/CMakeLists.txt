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

add_library(image_host STATIC
  base_image_host.cpp
  custom_image_host.cpp
  multipass_image_host.cpp
  ubuntu_image_host.cpp)

target_link_libraries(image_host PRIVATE
  fmt::fmt-header-only
  logger
  platform
  settings
  simplestreams
  utils
  Qt6::Core)
