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

# Copy gRPC port
file(COPY "${CMAKE_SOURCE_DIR}/3rd-party/vcpkg/ports/grpc" DESTINATION "${CMAKE_BINARY_DIR}/vcpkg-ports/")
# Rename the original port file
file(RENAME "${CMAKE_BINARY_DIR}/vcpkg-ports/grpc/portfile.cmake" "${CMAKE_BINARY_DIR}/vcpkg-ports/grpc/portfile-original.cmake")
# Copy our shim portfile
file(COPY "${CMAKE_SOURCE_DIR}/3rd-party/vcpkg-ports/grpc-portfile-wrapper.cmake" DESTINATION "${CMAKE_BINARY_DIR}/vcpkg-ports/grpc/")
file(RENAME "${CMAKE_BINARY_DIR}/vcpkg-ports/grpc/grpc-portfile-wrapper.cmake" "${CMAKE_BINARY_DIR}/vcpkg-ports/grpc/portfile.cmake")
# Copy the target filter hook
file(COPY "${CMAKE_SOURCE_DIR}/3rd-party/vcpkg-ports/multipass-vcpkg-target-filter-hook.cmake" DESTINATION "${CMAKE_BINARY_DIR}/vcpkg-ports/grpc")


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
