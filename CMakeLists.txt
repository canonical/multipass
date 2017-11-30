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

cmake_minimum_required(VERSION 3.1)

set(CMAKE_OSX_DEPLOYMENT_TARGET "10.10") # needs to be set before "project"

project(Multipass)

option(MULTIPASS_ENABLE_TESTS "Build tests" ON)

set(CMAKE_RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/bin)
set(CMAKE_LIBRARY_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/lib)
set(CMAKE_RUNTIME_OUTPUT_DIRECTORY_DEBUG "${CMAKE_RUNTIME_OUTPUT_DIRECTORY}")
set(CMAKE_RUNTIME_OUTPUT_DIRECTORY_RELEASE "${CMAKE_RUNTIME_OUTPUT_DIRECTORY}")

# Needs to be here before we set our own compilation options
add_subdirectory(3rd-party)

# Qt config
find_package(Qt5Core)
find_package(Qt5Network)

execute_process(COMMAND git describe --tags
                WORKING_DIRECTORY ${CMAKE_SOURCE_DIR}
                OUTPUT_VARIABLE MULTIPASS_VERSION
                OUTPUT_STRIP_TRAILING_WHITESPACE)

set(CMAKE_CXX_STANDARD 14)
if(MSVC)
  # Build with multiple processes
  add_definitions(/MP)
  add_definitions(-D_WIN32_WINNT=0xA00) # A gprc header requires this (specifies Windows 10)
  add_definitions(-DMULTIPASS_PLATFORM_WINDOWS)
  add_definitions(-DLIBSSH_STATIC) # otherwise adds declspec specifiers to libssh apis
elseif(APPLE)
  add_compile_options(-Werror -Wall -pedantic -fPIC)
  add_definitions(-DMULTIPASS_PLATFORM_APPLE)
else()
  add_compile_options(-Werror -Wall -pedantic -Wextra -Wno-unused-parameter -Wcast-align -Wempty-body -Wformat-security -Winit-self -Warray-bounds -Wno-expansion-to-defined -fPIC)
  add_definitions(-DMULTIPASS_PLATFORM_LINUX)
endif()

set(MULTIPASS_GENERATED_SOURCE_DIR ${CMAKE_BINARY_DIR}/gen)
file(MAKE_DIRECTORY ${MULTIPASS_GENERATED_SOURCE_DIR})

include_directories(
  include
  ${MULTIPASS_GENERATED_SOURCE_DIR})

add_subdirectory(src)

if(MULTIPASS_ENABLE_TESTS)
  add_subdirectory(tests)
endif()

include(packaging/cpack.cmake)
