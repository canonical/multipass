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
# Authored by: Gerry Boland <gerry.boland@canonical.com>

# Helpful docs:
# https://cmake.org/Wiki/CMake:Component_Install_With_CPack
# https://cmake.org/Wiki/CMake:CPackConfiguration
#
# Vital concept to understand are "Components" - a component is a collection of files to be installed.
# Any file to be packaged for installation must be part of a component - note the COMPONENT flag for
# "install". CPack then includes it in the generated package, to be installed at the destination
# specified in the "install" (plus a CPack-only prefix set below).
#
# Components are intended to be self-contained, as they may be installed individually. Components also
# smap directly to the options made visible to the user in the installer.
#

set(CPACK_WARN_ON_ABSOLUTE_INSTALL_DESTINATION ON) # helps avoid errors

set(CPACK_COMPONENTS_GROUPING ALL_COMPONENTS_IN_ONE)
set(CPACK_COMPONENTS_ALL multipassd multipass)

set(CPACK_COMPONENT_MULTIPASSD_DISPLAY_NAME "Multipass Daemon")
set(CPACK_COMPONENT_MULTIPASSD_DESCRIPTION
   "Background process that creates and manages virtual machines")
set(CPACK_COMPONENT_MULTIPASS_DISPLAY_NAME "Command line tooling (multipass)")
set(CPACK_COMPONENT_MULTIPASS_DESCRIPTION
   "Command line tool to talk to the multipass daemon")

set(CPACK_COMPONENT_MULTIPASSD_REQUIRED)

# set default CPack Packaging options
set(CPACK_PACKAGE_NAME              "multipass")
set(CPACK_PACKAGE_VENDOR            "canonical")
set(CPACK_PACKAGE_CONTACT           "contact@canonical.com")
set(CPACK_PACKAGE_VERSION           "${MULTIPASS_VERSION}")
#set(CPACK_PACKAGE_ICON              "${PROJECT_SOURCE_DIR}/cmake/sac_logo.png")

if (CMAKE_BUILD_TYPE STREQUAL "Release")
  set(CPACK_STRIP_FILES 1)
endif ()

# set (CPACK_PACKAGE_DESCRIPTION_FILE ...)
set(CPACK_PACKAGE_DESCRIPTION_SUMMARY "Easily create, control and connect to Ubuntu instances")
set(CPACK_RESOURCE_FILE_LICENSE "${PROJECT_SOURCE_DIR}/packaging/LICENCE.txt")
set(CPACK_RESOURCE_FILE_README "${PROJECT_SOURCE_DIR}/packaging/README.txt")
set(CPACK_RESOURCE_FILE_WELCOME "${PROJECT_SOURCE_DIR}/packaging/WELCOME.txt")



if(APPLE)
  set(CPACK_GENERATOR "productbuild")
  set(CPACK_productbuild_COMPONENT_INSTALL ON)

  set(CPACK_PACKAGING_INSTALL_PREFIX   "/Library/Application Support/com.canonical.multipass")

  set(MULTIPASSD_PLIST "com.canonical.multipassd.plist")
  configure_file("${CMAKE_SOURCE_DIR}/packaging/macos/${MULTIPASSD_PLIST}.in"
                 "${CMAKE_BINARY_DIR}/${MULTIPASSD_PLIST}" @ONLY)
  configure_file("${CMAKE_SOURCE_DIR}/packaging/macos/postinstall-multipassd.sh.in"
                 "${CMAKE_BINARY_DIR}/postinstall-multipassd.sh" @ONLY)
  configure_file("${CMAKE_SOURCE_DIR}/packaging/macos/postinstall-multipass.sh.in"
                 "${CMAKE_BINARY_DIR}/postinstall-multipass.sh" @ONLY)
  install(FILES "${CMAKE_BINARY_DIR}/${MULTIPASSD_PLIST}" DESTINATION Resources COMPONENT multipassd)
  install(DIRECTORY "${CMAKE_SOURCE_DIR}/completions" DESTINATION Resources COMPONENT multipass)

  set(CPACK_PREFLIGHT_MULTIPASSD_SCRIPT  "${CMAKE_SOURCE_DIR}/packaging/macos/preinstall-multipassd.sh")
  set(CPACK_POSTFLIGHT_MULTIPASSD_SCRIPT "${CMAKE_BINARY_DIR}/postinstall-multipassd.sh")
  set(CPACK_POSTFLIGHT_MULTIPASS_SCRIPT  "${CMAKE_BINARY_DIR}/postinstall-multipass.sh")

  # CPack doesn't support a direct way to customise the Distribution.dist file, but the template
  # CPack.distribution.dist.in is searched for in the CMAKE_MODULE_PATH before CMAKE_ROOT, so as a hack,
  # point it to a local directory with our custom template
  set(CMAKE_MODULE_PATH "${CMAKE_SOURCE_DIR}/packaging/macos")
  
  install(FILES "${CMAKE_SOURCE_DIR}/packaging/macos/uninstall.sh" 
          DESTINATION . COMPONENT multipassd)
endif()

# must be last
include(CPack)
