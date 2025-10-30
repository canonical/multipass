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

# Get list of portfile overrides
file(GLOB vcpkg_port_entries RELATIVE ${CMAKE_SOURCE_DIR}/3rd-party/vcpkg-ports ${CMAKE_SOURCE_DIR}/3rd-party/vcpkg-ports/*)
set(WRAPPED_VCPKG_PACKAGES "")

foreach(entry ${vcpkg_port_entries})
    if(IS_DIRECTORY ${CMAKE_SOURCE_DIR}/3rd-party/vcpkg-ports/${entry})
        list(APPEND WRAPPED_VCPKG_PACKAGES ${entry})
    endif()
endforeach()

set(VCPKG_OVERLAY_PORTS "" CACHE STRING "vcpkg Overlay Ports (dynamically populated)" FORCE)

# Iterate over all wrapped packages to set up the wrapper and the hook.
foreach(PACKAGE ${WRAPPED_VCPKG_PACKAGES})
  message(STATUS "Copying the original vcpkg port for ${PACKAGE} to override.")
  # Copy the port to build/ folder
  file(COPY "${CMAKE_SOURCE_DIR}/3rd-party/vcpkg/ports/${PACKAGE}" DESTINATION "${CMAKE_BINARY_DIR}/vcpkg-ports/")
  # Rename the original portfile
  file(RENAME "${CMAKE_BINARY_DIR}/vcpkg-ports/${PACKAGE}/portfile.cmake" "${CMAKE_BINARY_DIR}/vcpkg-ports/${PACKAGE}/portfile-original.cmake")
  # Copy the portfile wrapper
  file(COPY "${CMAKE_SOURCE_DIR}/3rd-party/vcpkg-ports/${PACKAGE}/portfile-wrapper.cmake" DESTINATION "${CMAKE_BINARY_DIR}/vcpkg-ports/${PACKAGE}/")
  # Rename the portfile-wrapper.cmake to portfile.cmake
  file(RENAME "${CMAKE_BINARY_DIR}/vcpkg-ports/${PACKAGE}/portfile-wrapper.cmake" "${CMAKE_BINARY_DIR}/vcpkg-ports/${PACKAGE}/portfile.cmake")
  # Copy the target filter hook
  file(COPY "${CMAKE_SOURCE_DIR}/3rd-party/vcpkg-ports/multipass-vcpkg-target-filter-hook.cmake" DESTINATION "${CMAKE_BINARY_DIR}/vcpkg-ports/${PACKAGE}")
  # Copy the override vcpkg function module
  file(COPY "${CMAKE_SOURCE_DIR}/3rd-party/vcpkg-ports/multipass-override-vcpkg-function.cmake" DESTINATION "${CMAKE_BINARY_DIR}/vcpkg-ports/${PACKAGE}")
  # Add it to overlay ports
  list(APPEND VCPKG_OVERLAY_PORTS "${CMAKE_BINARY_DIR}/vcpkg-ports/${PACKAGE}")
endforeach()

if("${HOST_OS_NAME}" STREQUAL "macOS")
  # needs to be set before "project"
  set(VCPKG_HOST_OS "osx")
  if (CMAKE_HOST_SYSTEM_PROCESSOR STREQUAL "arm64")
    set(CMAKE_OSX_DEPLOYMENT_TARGET "14.0" CACHE STRING "macOS Deployment Target")
  else()
    set(CMAKE_OSX_DEPLOYMENT_TARGET "13.3" CACHE STRING "macOS Deployment Target")
  endif()
elseif("${HOST_OS_NAME}" STREQUAL "Windows")
  set(VCPKG_HOST_OS "windows-static-md")
else()
  set(VCPKG_HOST_OS "linux")
endif()

if(NOT DEFINED VCPKG_BUILD_DEFAULT)
  cmake_host_system_information(RESULT VCPKG_HOST_ARCH QUERY OS_PLATFORM)
  if("${VCPKG_HOST_ARCH}" STREQUAL "x86_64" OR "${VCPKG_HOST_ARCH}" STREQUAL "AMD64")
    set(VCPKG_HOST_ARCH "x64")
  elseif("${VCPKG_HOST_ARCH}" STREQUAL "aarch64")
    set(VCPKG_HOST_ARCH "arm64")
  endif()

  set(VCPKG_HOST_TRIPLET "${VCPKG_HOST_ARCH}-${VCPKG_HOST_OS}-release")
  set(VCPKG_TARGET_TRIPLET "${VCPKG_HOST_ARCH}-${VCPKG_HOST_OS}-release")
endif()

message(STATUS "Bootstrapping vcpkg...")

if(CMAKE_HOST_SYSTEM_NAME STREQUAL "Windows")
  set(VCPKG_BOOTSTRAP_EXTENSION "bat")
else()
  set(VCPKG_BOOTSTRAP_EXTENSION "sh")
endif()

execute_process(
  COMMAND "${CMAKE_CURRENT_SOURCE_DIR}/3rd-party/vcpkg/bootstrap-vcpkg.${VCPKG_BOOTSTRAP_EXTENSION}"
  RESULT_VARIABLE VCPKG_BOOTSTRAP_RESULT
)

if(NOT VCPKG_BOOTSTRAP_RESULT EQUAL 0)
  message(FATAL_ERROR "Bootstrapping vcpkg failed with exit code: ${VCPKG_BOOTSTRAP_RESULT}")
else()
  message(STATUS "Bootstrapping vcpkg completed successfully.")
endif()

set(CMAKE_TOOLCHAIN_FILE "${CMAKE_CURRENT_SOURCE_DIR}/3rd-party/vcpkg/scripts/buildsystems/vcpkg.cmake"
  CACHE STRING "Vcpkg toolchain file")
