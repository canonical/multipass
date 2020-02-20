# Copyright © 2017-2019 Canonical Ltd.
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

cmake_minimum_required(VERSION 3.1)

project(Multipass)

option(MULTIPASS_ENABLE_TESTS "Build tests" ON)

include(GNUInstallDirs)

set(CMAKE_RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/bin)
set(CMAKE_LIBRARY_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/lib)
set(CMAKE_RUNTIME_OUTPUT_DIRECTORY_DEBUG "${CMAKE_RUNTIME_OUTPUT_DIRECTORY}")
set(CMAKE_RUNTIME_OUTPUT_DIRECTORY_RELEASE "${CMAKE_RUNTIME_OUTPUT_DIRECTORY}")

string(TOLOWER "${CMAKE_BUILD_TYPE}" cmake_build_type_lower)

if (CMAKE_CXX_COMPILER_ID STREQUAL "Clang")
  if(cmake_build_type_lower MATCHES "asan")
    add_compile_options(-fno-omit-frame-pointer -fsanitize=address)
    set (CMAKE_EXE_LINKER_FLAGS "${CMAKE_EXE_LINKER_FLAGS} -fno-omit-frame-pointer -fsanitize=address")
  elseif(cmake_build_type_lower MATCHES "ubsan")
    add_compile_options(-fno-omit-frame-pointer -fsanitize=undefined)
    set (CMAKE_EXE_LINKER_FLAGS "${CMAKE_EXE_LINKER_FLAGS} -fno-omit-frame-pointer -fsanitize=undefined")
  elseif(cmake_build_type_lower MATCHES "tsan")
    add_compile_options(-fno-omit-frame-pointer -fsanitize=thread)
    set (CMAKE_EXE_LINKER_FLAGS "${CMAKE_EXE_LINKER_FLAGS} -fno-omit-frame-pointer -fsanitize=thread")
  endif()
endif()

# these we want to apply even to 3rd-party
if(cmake_build_type_lower MATCHES "coverage")
  if(CMAKE_CXX_COMPILER_ID STREQUAL "GNU")
    add_compile_options(--coverage)
    if(COMMAND add_link_options)
      add_link_options(--coverage)
    else()
      string(APPEND CMAKE_SHARED_LINKER_FLAGS " --coverage")
      string(APPEND CMAKE_EXE_LINKER_FLAGS " --coverage")
    endif()
  else()
    message(FATAL_ERROR "Coverage build only supported with GCC")
  endif()
endif()

# Needs to be here before we set further compilation options
add_subdirectory(3rd-party)

# Qt config
find_package(Qt5 COMPONENTS Core Network Widgets REQUIRED)

function(determine_version OUTPUT_VARIABLE)
  # use upstream repo as the authoritative reference when checking for release status
  # set -DMULTIPASS_UPSTREAM="" to use the local repository
  if(MULTIPASS_UPSTREAM)
    set(MULTIPASS_UPSTREAM "${MULTIPASS_UPSTREAM}/")
  endif()

  execute_process(COMMAND git describe --all --exact --match "${MULTIPASS_UPSTREAM}release/*"
                  WORKING_DIRECTORY ${CMAKE_SOURCE_DIR}
                  OUTPUT_VARIABLE GIT_RELEASE_BRANCH
                  OUTPUT_STRIP_TRAILING_WHITESPACE
                  ERROR_QUIET)

  execute_process(COMMAND git describe --long
                  WORKING_DIRECTORY ${CMAKE_SOURCE_DIR}
                  OUTPUT_VARIABLE GIT_VERSION
                  OUTPUT_STRIP_TRAILING_WHITESPACE)

  # only use -rc tags on release/* branches
  string(REGEX MATCH "release/[0-9]+.[0-9]+" GIT_RELEASE_MATCH "${GIT_RELEASE_BRANCH}")
  if(GIT_RELEASE_MATCH)
      if(NOT DEFINED MULTIPASS_UPSTREAM)
        message(FATAL_ERROR "You need to set MULTIPASS_UPSTREAM for a release build.\
                             \nUse an empty string to make local the authoritative repository.")
      endif()

      execute_process(COMMAND git describe --exact
                      WORKING_DIRECTORY ${CMAKE_SOURCE_DIR}
                      OUTPUT_VARIABLE GIT_RELEASE
                      OUTPUT_STRIP_TRAILING_WHITESPACE
                      ERROR_QUIET)

      execute_process(COMMAND git describe --tags --match *-rc --abbrev=0
                      WORKING_DIRECTORY ${CMAKE_SOURCE_DIR}
                      OUTPUT_VARIABLE GIT_TAG
                      OUTPUT_STRIP_TRAILING_WHITESPACE)
  else()
      execute_process(COMMAND git describe --tags --match *-dev --abbrev=0
                      WORKING_DIRECTORY ${CMAKE_SOURCE_DIR}
                      OUTPUT_VARIABLE GIT_TAG
                      OUTPUT_STRIP_TRAILING_WHITESPACE)
  endif()

  string(REGEX MATCH "^v.+-([0-9]+)-(g.+)$" GIT_VERSION_MATCH ${GIT_VERSION})

  if(GIT_RELEASE)
    set(NEW_VERSION ${GIT_RELEASE})
  elseif(GIT_VERSION_MATCH AND MULTIPASS_BUILD_LABEL)
    set(NEW_VERSION ${GIT_TAG}.${CMAKE_MATCH_1}.${MULTIPASS_BUILD_LABEL}+${CMAKE_MATCH_2})
  elseif(GIT_VERSION_MATCH)
    set(NEW_VERSION ${GIT_TAG}.${CMAKE_MATCH_1}+${CMAKE_MATCH_2})
  else()
    message(FATAL_ERROR "Failed to parse version number: ${GIT_VERSION}")
  endif()

  if(APPLE OR MSVC)
    string(REGEX MATCH "\\+full$" FULL_SUFFIX_MATCH ${NEW_VERSION})
    if(NOT FULL_SUFFIX_MATCH)
      set(NEW_VERSION ${NEW_VERSION}.full)
    endif()
    if(APPLE)
      string(REPLACE "full" "mac" NEW_VERSION ${NEW_VERSION})
    else()
      string(REPLACE "full" "win" NEW_VERSION ${NEW_VERSION})
    endif()
  endif()

  string(REGEX MATCH "^v(.+)" VERSION_MATCH ${NEW_VERSION})

  if(VERSION_MATCH)
    set(${OUTPUT_VARIABLE} ${CMAKE_MATCH_1} PARENT_SCOPE)
  else()
    message(FATAL_ERROR "Invalid tag detected: ${NEW_VERSION}")
  endif()
endfunction()

determine_version(MULTIPASS_VERSION)
set(MULTIPASS_VERSION ${MULTIPASS_VERSION})
message(STATUS "Setting version to: ${MULTIPASS_VERSION}")

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED TRUE)
set(CMAKE_CXX_EXTENSIONS OFF)

add_compile_options(-Werror -Wall -pedantic -Wextra -Wempty-body -Wformat-security -Winit-self -Warray-bounds -fPIC)

if(NOT ${CMAKE_HOST_SYSTEM_PROCESSOR} MATCHES "^arm")
  add_compile_options(-Wcast-align)
endif()

CHECK_CXX_COMPILER_FLAG("-Wno-expansion-to-defined" COMPILER_SUPPORTS_NO_EXP_TO_DEFINED)
if(COMPILER_SUPPORTS_NO_EXP_TO_DEFINED)
  add_compile_options(-Wno-expansion-to-defined)
endif()
add_definitions(-DMULTIPASS_PLATFORM_LINUX)

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
        ${CMAKE_SOURCE_DIR}'/tests/*'
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

add_subdirectory(data)
add_subdirectory(src)

if(MULTIPASS_ENABLE_TESTS)
  enable_testing()
  add_subdirectory(tests)
endif()
