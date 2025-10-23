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
  daemon.cpp
  daemon_config.cpp
  daemon_init_settings.cpp
  daemon_rpc.cpp
  default_vm_image_vault.cpp
  instance_settings_handler.cpp
  runtime_instance_info_helper.cpp
  snapshot_settings_handler.cpp)

include_directories(daemon
  ${CMAKE_SOURCE_DIR}/src/platform/backends)

target_link_libraries(daemon
  cert
  delayed_shutdown
  fmt::fmt-header-only
  image_host
  logger
  petname
  platform
  rpc
  settings
  ssh
  sshfs_mount
  utils
  Qt6::Core
  Qt6::Network
  xz_image_decoder
  yaml-cpp::yaml-cpp)

add_library(delayed_shutdown STATIC
  delayed_shutdown_timer.cpp
  ${CMAKE_SOURCE_DIR}/include/multipass/delayed_shutdown_timer.h)

target_link_libraries(delayed_shutdown
  fmt::fmt-header-only
  logger
  ssh_common
  Qt6::Core
)

if(MSVC)
  add_executable(multipassd daemon_main_win.cpp)
  qt6_disable_unicode_defines(multipassd)
else()
  add_executable(multipassd daemon_main.cpp)
endif()

target_link_libraries(multipassd
  fmt::fmt-header-only
  daemon)

# this TLS plugin is common to all platforms, so we can retrieve it
find_package(Qt6QTlsBackendCertOnlyPlugin PATHS ${Qt6Network_DIR})
# the IMPORTED_LOCATION is defined for RELEASE on all platforms, so we use that to get the path of the shared lib itself
get_target_property(QT_TLS_PLUGIN_PATH Qt6::QTlsBackendCertOnlyPlugin IMPORTED_LOCATION_RELEASE)
# the path of the root plugins dir is two dirs up from the path of the shared lib we just retrieved
cmake_path(GET QT_TLS_PLUGIN_PATH PARENT_PATH QT_TLS_PLUGIN_DIR)
cmake_path(GET QT_TLS_PLUGIN_DIR PARENT_PATH QT_PLUGINS_DIR)
# this way of retrieving the Qt plugins directory is kind of a hack
# this information was extracted from Qt's cmake files, but it has no stability guarantee

if (NOT IS_DIRECTORY ${QT_PLUGINS_DIR}/tls)
    message(FATAL_ERROR "Failed to retrieve Qt 'tls' plugin directory")
endif()

if (NOT IS_DIRECTORY ${QT_PLUGINS_DIR}/networkinformation)
    message(FATAL_ERROR "Failed to retrieve Qt 'networkinformation' plugin directory")
endif()

if(APPLE)
  set_target_properties( multipassd PROPERTIES INSTALL_RPATH "@executable_path/../lib" )

  # Copy any framework depenencies into the install directory for packaging
  install(CODE "
    include(${CMAKE_SOURCE_DIR}/packaging/FrameworkUtilities.cmake)
    install_frameworks(\"${CMAKE_RUNTIME_OUTPUT_DIRECTORY}/multipassd\" \"\$ENV{DESTDIR}\${CMAKE_INSTALL_PREFIX}/lib\")
    " COMPONENT multipassd)

  file(GLOB QT_TLS_PLUGIN_FILES "${QT_PLUGINS_DIR}/tls/*.dylib")
  install(FILES ${QT_TLS_PLUGIN_FILES} DESTINATION plugins/tls COMPONENT multipassd)

  file(GLOB QT_NETWORKINFORMATION_PLUGIN_FILES "${QT_PLUGINS_DIR}/networkinformation/*.dylib")
  install(FILES ${QT_NETWORKINFORMATION_PLUGIN_FILES} DESTINATION plugins/networkinformation COMPONENT multipassd)
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

  file(GLOB QT_TLS_PLUGIN_FILES "${QT_PLUGINS_DIR}/tls/*.dll")
  install(FILES ${QT_TLS_PLUGIN_FILES} DESTINATION bin/tls COMPONENT multipassd)

  file(GLOB QT_NETWORKINFORMATION_PLUGIN_FILES "${QT_PLUGINS_DIR}/networkinformation/*.dll")
  install(FILES ${QT_NETWORKINFORMATION_PLUGIN_FILES} DESTINATION bin/networkinformation COMPONENT multipassd)
endif()
