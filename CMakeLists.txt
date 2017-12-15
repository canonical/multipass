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

execute_process(COMMAND git describe
                WORKING_DIRECTORY ${CMAKE_SOURCE_DIR}
                OUTPUT_VARIABLE MULTIPASS_VERSION
                OUTPUT_STRIP_TRAILING_WHITESPACE)

set(CMAKE_CXX_STANDARD 14)
add_compile_options(-Werror -Wall -pedantic -Wextra -Wno-unused-parameter -Wcast-align -Wempty-body -Wformat-security -Winit-self -Warray-bounds -Wno-expansion-to-defined -fPIC)
add_definitions(-DMULTIPASS_PLATFORM_LINUX)

string(TOLOWER "${CMAKE_BUILD_TYPE}" cmake_build_type_lower)

if(cmake_build_type_lower MATCHES "coverage")
  find_program(GCOV gcov)
  find_program(LCOV lcov)
  find_program(GENHTML genhtml)

  if(NOT (GCOV AND LCOV AND GENHTML))
    message(AUTHOR_WARNING
      "gcov, lcov and genhtml required for coverage reports. Disabling."
    )
  else()
    message(STATUS
      "Coverage enabled, use the 'covreport' target."
    )
    add_custom_target(covreport
      DEPENDS multipass_tests
      WORKING_DIRECTORY ${CMAKE_BUILD_DIR}
      COMMAND ${LCOV} --directory . --zerocounters
      COMMAND ${CMAKE_COMMAND} --build ${CMAKE_BINARY_DIR} --target test
      COMMAND ${LCOV} --directory . --capture --output-file coverage.info
      COMMAND ${LCOV}
        --remove coverage.info
        '/usr/*'
        ${CMAKE_SOURCE_DIR}'/3rd-party/*'
        ${CMAKE_BINARY_DIR}
        --output-file coverage.cleaned
      COMMAND ${CMAKE_COMMAND} -E remove coverage.info
      COMMAND ${GENHTML} -o coverage coverage.cleaned
    )
  endif()
endif()

set(MULTIPASS_GENERATED_SOURCE_DIR ${CMAKE_BINARY_DIR}/gen)
file(MAKE_DIRECTORY ${MULTIPASS_GENERATED_SOURCE_DIR})

include_directories(
  include
  ${MULTIPASS_GENERATED_SOURCE_DIR})

add_subdirectory(src)

if(MULTIPASS_ENABLE_TESTS)
  enable_testing()
  add_subdirectory(tests)
endif()
