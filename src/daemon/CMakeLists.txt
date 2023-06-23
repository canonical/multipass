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

add_library(daemon STATIC
  cli.cpp
  common_image_host.cpp
  custom_image_host.cpp
  daemon.cpp
  daemon_config.cpp
  daemon_init_settings.cpp
  daemon_rpc.cpp
  default_vm_image_vault.cpp
  instance_settings_handler.cpp
  ubuntu_image_host.cpp)

include_directories(daemon
  ${CMAKE_SOURCE_DIR}/src/platform/backends)

target_link_libraries(daemon
  cert
  delayed_shutdown
  fmt
  logger
  petname
  platform
  rpc
  settings
  simplestreams
  ssh
  sshfs_mount
  utils
  Qt5::Core
  Qt5::Network
  blueprint_provider
  xz_image_decoder
  yaml)

add_library(delayed_shutdown STATIC
  delayed_shutdown_timer.cpp
  ${CMAKE_SOURCE_DIR}/include/multipass/delayed_shutdown_timer.h)

target_link_libraries(delayed_shutdown
  fmt
  logger
  ssh_common
  Qt5::Core
)

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

if(MSVC)
  install(CODE "
    include(BundleUtilities)
    fixup_bundle(\"\${CMAKE_INSTALL_PREFIX}/bin/multipassd.exe\"  \"\"  \"${CMAKE_RUNTIME_OUTPUT_DIRECTORY}\")
    " COMPONENT multipassd)

  file(GLOB SSL_DLLS "${OPENSSL_INCLUDE_DIR}/../*.dll")
  install(FILES ${SSL_DLLS} DESTINATION bin COMPONENT multipassd)
endif()
