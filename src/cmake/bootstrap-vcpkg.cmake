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

# Configure and bootstrap vcpkg.

cmake_host_system_information(RESULT HOST_OS_NAME QUERY OS_NAME)
cmake_host_system_information(RESULT HOST_ARCH QUERY OS_PLATFORM)

if ("${HOST_ARCH}" STREQUAL "arm64" OR "${HOST_ARCH}" STREQUAL "aarch64")
  set(VCPKG_HOST_ARCH "arm64")
else()
  set(VCPKG_HOST_ARCH "x64")
endif()

if("${HOST_OS_NAME}" STREQUAL "macOS")
  # needs to be set before "project"
  set(VCPKG_HOST_OS "osx")
  if ("${VCPKG_HOST_ARCH}" STREQUAL "arm64")
    set(CMAKE_OSX_DEPLOYMENT_TARGET "14.0" CACHE STRING "macOS Deployment Target")
  else()
    set(CMAKE_OSX_DEPLOYMENT_TARGET "13.3" CACHE STRING "macOS Deployment Target")
  endif()
elseif("${HOST_OS_NAME}" STREQUAL "Windows")
  set(VCPKG_HOST_OS "windows-static-md")
else()
  set(VCPKG_HOST_OS "linux")
endif()

if(NOT VCPKG_BUILD_DEFAULT)
  message(STATUS "Will build `vcpkg` dependencies in `release-only` mode.")

  # Propagate macOS deployment target to vcpkg triplet
  if(APPLE AND DEFINED CMAKE_OSX_DEPLOYMENT_TARGET)
    set(VCPKG_TRIPLETS_DIR "${CMAKE_CURRENT_BINARY_DIR}/vcpkg-overlays/triplets")
    configure_file(
      "${CMAKE_CURRENT_SOURCE_DIR}/3rd-party/vcpkg-overlays/triplets/osx-release.cmake.in"
      "${VCPKG_TRIPLETS_DIR}/${VCPKG_HOST_ARCH}-${VCPKG_HOST_OS}-release.cmake"
      @ONLY
    )
    set(VCPKG_OVERLAY_TRIPLETS "${VCPKG_TRIPLETS_DIR}" CACHE STRING "Custom vcpkg triplets directory")
    message(STATUS "Generated vcpkg triplet: ${VCPKG_HOST_ARCH}-${VCPKG_HOST_OS}-release.cmake with deployment target ${CMAKE_OSX_DEPLOYMENT_TARGET}")
  endif()

  set(VCPKG_HOST_TRIPLET "${VCPKG_HOST_ARCH}-${VCPKG_HOST_OS}-release")
  set(VCPKG_TARGET_TRIPLET "${VCPKG_HOST_ARCH}-${VCPKG_HOST_OS}-release")
else()
  message(NOTICE "Will build `vcpkg` deps in both `debug` and `release` configurations. Be aware that it will take around twice the time to build the `vcpkg` deps.")
endif()

message(STATUS "Bootstrapping vcpkg, triplet: ${VCPKG_TARGET_TRIPLET}...")


set(MULTIPASS_VCPKG_LOCATION "${CMAKE_CURRENT_SOURCE_DIR}/3rd-party/vcpkg" CACHE PATH
  "Root vcpkg location, where the top bootstrap scripts are located")

set(VCPKG_CMD "${MULTIPASS_VCPKG_LOCATION}/bootstrap-vcpkg.")
if (CMAKE_HOST_SYSTEM_NAME STREQUAL "Windows")
  string(APPEND VCPKG_CMD "bat")
  list(PREPEND VCPKG_CMD "cmd" "/c") # run in cmd; otherwise, we'd fail when called from PowerShell
else ()
  string(APPEND VCPKG_CMD "sh")
endif ()

execute_process(COMMAND ${VCPKG_CMD} RESULT_VARIABLE VCPKG_BOOTSTRAP_RESULT)

if(NOT VCPKG_BOOTSTRAP_RESULT EQUAL 0)
  message(FATAL_ERROR "Bootstrapping vcpkg failed with exit code: ${VCPKG_BOOTSTRAP_RESULT}")
else()
  message(STATUS "Bootstrapping vcpkg completed successfully.")
endif()

if(MULTIPASS_ENABLE_TESTS)
  set(VCPKG_MANIFEST_FEATURES "tests" CACHE STRING "Enabled vcpkg features")
endif()

set(CMAKE_TOOLCHAIN_FILE "${CMAKE_CURRENT_SOURCE_DIR}/3rd-party/vcpkg/scripts/buildsystems/vcpkg.cmake"
  CACHE STRING "Vcpkg toolchain file")
