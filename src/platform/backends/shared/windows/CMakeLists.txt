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

set (CMAKE_AUTOMOC ON)
add_library(shared_win STATIC
  aes.cpp
  powershell.cpp
  process_factory.cpp
  smb_mount_handler.cpp)

include_directories(shared_win
  ..)

target_link_libraries(shared_win
  Qt6::Core
  fmt
  logger
  OpenSSL::Crypto
  sftp_client
  utils)
