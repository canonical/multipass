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

add_library(network STATIC
            local_socket_reply.cpp
            network_access_manager.cpp
            url_downloader.cpp
            ${CMAKE_SOURCE_DIR}/include/multipass/network_access_manager.h
            local_socket_reply.h)

add_library(ip_address STATIC
            ip_address.cpp)

target_link_libraries(network
  fmt
  logger
  utils
  Qt6::Core
  Qt6::Network)
