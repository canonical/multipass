# Copyright © 2017 Canonical Ltd.
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
# Authored by: Alberto Aguirre <alberto.aguirre@canonical.com>

set(CMAKE_AUTOMOC ON)

add_library(daemon STATIC
  daemon.cpp
  daemon_config.cpp
  daemon_rpc.cpp
  default_vm_image_vault.cpp
  json_writer.cpp
  ubuntu_image_host.cpp)

target_link_libraries(daemon
  fmt
  iso
  rpc
  petname
  platform
  simplestreams
  ssh
  sshfs_mount
  utils
  Qt5::Core
  Qt5::Network
  yaml)

if(MSVC)
  add_executable(multipassd daemon_main_win.cpp)
else()
  add_executable(multipassd daemon_main.cpp)
endif()

target_link_libraries(multipassd
  fmt
  daemon)

if(APPLE)
  set_target_properties( multipassd PROPERTIES INSTALL_RPATH "@executable_path/../lib" )

  # Copy any framework depenencies into the install directory for packaging
  install(CODE "
    include(${CMAKE_SOURCE_DIR}/packaging/FrameworkUtilities.cmake)
    install_frameworks(\"${CMAKE_RUNTIME_OUTPUT_DIRECTORY}/multipassd\" \"\$ENV{DESTDIR}\${CMAKE_INSTALL_PREFIX}/lib\")
    " COMPONENT multipassd)
endif(APPLE)

install(TARGETS multipassd
  DESTINATION bin
  COMPONENT multipassd)
