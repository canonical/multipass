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

cmake_minimum_required(VERSION 3.17)
cmake_policy(SET CMP0079 NEW) # Allow target_link_libraries() in subdirs

cmake_host_system_information(RESULT HOST_OS_NAME QUERY OS_NAME)

if("${HOST_OS_NAME}" STREQUAL "macOS")
  set(CMAKE_OSX_DEPLOYMENT_TARGET "12.0") # needs to be set before "project"
  set(VCPKG_HOST_OS "osx")
elseif("${HOST_OS_NAME}" STREQUAL "Windows")
  set(VCPKG_HOST_OS "windows-static-md")
else()
  set(VCPKG_HOST_OS "linux")
endif()

# Use ccache if it's installed
find_program(CCACHE_PROGRAM ccache)
if(CCACHE_PROGRAM)
  set(CMAKE_CXX_COMPILER_LAUNCHER "${CCACHE_PROGRAM}")
  set(CMAKE_C_COMPILER_LAUNCHER "${CCACHE_PROGRAM}")
endif()

if("${CMAKE_BUILD_TYPE}" STREQUAL "Debug")
  set(VCPKG_BUILD_DEFAULT TRUE)
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

set(CMAKE_TOOLCHAIN_FILE "${CMAKE_CURRENT_SOURCE_DIR}/3rd-party/vcpkg/scripts/buildsystems/vcpkg.cmake"
  CACHE STRING "Vcpkg toolchain file")

project(Multipass)

option(MULTIPASS_ENABLE_TESTS "Build tests" ON)
option(MULTIPASS_ENABLE_FLUTTER_GUI "Build Flutter GUI" ON)

include(GNUInstallDirs)

set(CMAKE_RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/bin)
set(CMAKE_LIBRARY_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/lib)
set(CMAKE_RUNTIME_OUTPUT_DIRECTORY_DEBUG "${CMAKE_RUNTIME_OUTPUT_DIRECTORY}")
set(CMAKE_RUNTIME_OUTPUT_DIRECTORY_RELEASE "${CMAKE_RUNTIME_OUTPUT_DIRECTORY}")

if(MULTIPASS_ENABLE_FLUTTER_GUI)
  if(MSVC)
    set(FLUTTER_TOOLS_EXTENSION ".bat")
  endif()
  set(FLUTTER_EXECUTABLE ${CMAKE_SOURCE_DIR}/3rd-party/flutter/bin/flutter${FLUTTER_TOOLS_EXTENSION})
  set(DART_EXECUTABLE ${CMAKE_SOURCE_DIR}/3rd-party/flutter/bin/dart${FLUTTER_TOOLS_EXTENSION})
  if(MSVC)
    set(FLUTTER_CONFIG_ARGS --no-enable-linux-desktop --no-enable-macos-desktop --enable-windows-desktop)
  elseif(APPLE)
    set(FLUTTER_CONFIG_ARGS --no-enable-linux-desktop --enable-macos-desktop --no-enable-windows-desktop)
  elseif(UNIX AND NOT APPLE)
    set(FLUTTER_CONFIG_ARGS --enable-linux-desktop --no-enable-macos-desktop --no-enable-windows-desktop)
  endif()
  execute_process(COMMAND ${FLUTTER_EXECUTABLE} config ${FLUTTER_CONFIG_ARGS} --suppress-analytics --no-enable-web --no-enable-android --no-enable-ios --no-enable-fuchsia --no-enable-custom-devices
                  COMMAND_ERROR_IS_FATAL ANY)
  execute_process(COMMAND ${FLUTTER_EXECUTABLE} precache ${FLUTTER_PRECACHE_OS}
                  COMMAND_ERROR_IS_FATAL ANY)
  execute_process(COMMAND ${DART_EXECUTABLE} pub get
                  WORKING_DIRECTORY ${CMAKE_SOURCE_DIR}/3rd-party/protobuf.dart/protoc_plugin
                  COMMAND_ERROR_IS_FATAL ANY)
  execute_process(COMMAND ${DART_EXECUTABLE} compile exe bin/protoc_plugin.dart
                  WORKING_DIRECTORY ${CMAKE_SOURCE_DIR}/3rd-party/protobuf.dart/protoc_plugin
                  COMMAND_ERROR_IS_FATAL ANY)
  set(DART_PROTOC_PLUGIN ${CMAKE_SOURCE_DIR}/3rd-party/protobuf.dart/protoc_plugin/bin/protoc_plugin.exe)
endif(MULTIPASS_ENABLE_FLUTTER_GUI)

string(TOLOWER "${CMAKE_BUILD_TYPE}" cmake_build_type_lower)

if (CMAKE_CXX_COMPILER_ID STREQUAL "Clang")
  add_definitions(-DMULTIPASS_COMPILER_CLANG)

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

# OpenSSL config
find_package(OpenSSL REQUIRED)

# gRPC config
find_package(gRPC CONFIG REQUIRED)

# Needs to be here before we set further compilation options
add_subdirectory(3rd-party)

# Qt config
find_package(Qt6 COMPONENTS Core Concurrent Network REQUIRED)

# POCO config
find_package(Poco REQUIRED COMPONENTS Foundation Zip)

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

      execute_process(COMMAND git describe --tags --match *-rc[0-9]* --abbrev=0
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

function(determine_version_components VERSION_STRING SEMANTIC_VERSION BUILD_NUMBER)
  # stuff before + is the version and after the + is the build metadata
  string(FIND "${VERSION_STRING}" "+" PLUS_POS)
  string(SUBSTRING "${VERSION_STRING}" 0 ${PLUS_POS} MULTIPASS_SEMANTIC_VERSION)
  if (PLUS_POS GREATER -1)
      string(SUBSTRING "${VERSION_STRING}" ${PLUS_POS} -1 MULTIPASS_BUILD_STRING)
      # if the build metadata starts with a g, the hexadecimal chars after that are a commit hash
      # otherwise, we do not derive any build string
      string(REGEX MATCH "^[+]g([a-f0-9]+)" "" "${MULTIPASS_BUILD_STRING}")
      set(BUILD_NUMBER_HEX ${CMAKE_MATCH_1})
  endif()
  # convert the hexadecimal commit hash to decimal. if none was extracted, this defaults to 0
  # we need it to be decimal because flutter --build-number accepts only decimal chars
  math(EXPR MULTIPASS_BUILD_NUMBER "0x0${BUILD_NUMBER_HEX}")

  set(${SEMANTIC_VERSION} ${MULTIPASS_SEMANTIC_VERSION} PARENT_SCOPE)
  set(${BUILD_NUMBER} ${MULTIPASS_BUILD_NUMBER} PARENT_SCOPE)
endfunction()

determine_version(MULTIPASS_VERSION)
set(MULTIPASS_VERSION ${MULTIPASS_VERSION})
message(STATUS "Setting version to: ${MULTIPASS_VERSION}")

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED TRUE)
set(CMAKE_CXX_EXTENSIONS OFF)
if(MSVC)
  add_definitions(-DMULTIPASS_COMPILER_MSVC)
  add_definitions(-D_WIN32_WINNT=0xA00) # A gprc header requires this (specifies Windows 10)
  add_definitions(-DNOMINMAX) # otherwise windows defines macros which interfere with std::min, std::max
  add_definitions(-DMULTIPASS_PLATFORM_WINDOWS)
  add_definitions(-DLIBSSH_STATIC) # otherwise adds declspec specifiers to libssh apis
  add_definitions(-D_SILENCE_ALL_CXX17_DEPRECATION_WARNINGS)
  add_definitions(-DWIN32_LEAN_AND_MEAN)
else()
  add_compile_options(-Werror -Wall -pedantic -fPIC -Wno-error=deprecated-declarations)

  if(APPLE)
    add_definitions(-DMULTIPASS_PLATFORM_APPLE)
    add_definitions(-DMULTIPASS_COMPILER_APPLE_CLANG)
  else()
    # Linux
    if(CMAKE_CXX_COMPILER_ID STREQUAL "GNU")
      add_definitions(-DMULTIPASS_COMPILER_GCC)
    endif()

    add_compile_options(-Wextra -Wempty-body -Wformat-security -Winit-self -Warray-bounds $<$<COMPILE_LANGUAGE:CXX>:-Wnon-virtual-dtor>)

    if(NOT ${CMAKE_HOST_SYSTEM_PROCESSOR} MATCHES "^arm")
      add_compile_options(-Wcast-align)
    endif()

    CHECK_CXX_COMPILER_FLAG("-Wno-expansion-to-defined" COMPILER_SUPPORTS_NO_EXP_TO_DEFINED)
    if(COMPILER_SUPPORTS_NO_EXP_TO_DEFINED)
      add_compile_options(-Wno-expansion-to-defined)
    endif()

    add_definitions(-DMULTIPASS_PLATFORM_LINUX)
    set(MULTIPASS_BACKENDS libvirt lxd)

    if (${CMAKE_HOST_SYSTEM_PROCESSOR} MATCHES "x86_64")
      list(APPEND MULTIPASS_BACKENDS qemu)
    endif()

    set(MULTIPASS_PLATFORM linux)
    set(LINUX TRUE)
  endif()
endif()

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
    execute_process(COMMAND ${LCOV} --version
                    OUTPUT_VARIABLE LCOV_VERSION_RAW)
    string(REGEX REPLACE "lcov: LCOV version " "" LCOV_VERSION_STR "${LCOV_VERSION_RAW}")
    string(REGEX REPLACE "\\..*" "" LCOV_VERSION_MAJOR "${LCOV_VERSION_STR}")
    if(${LCOV_VERSION_MAJOR} GREATER_EQUAL 2)
      set(LCOV_IGNORE_ERRORS --ignore-errors mismatch --ignore-errors negative)
    endif()

    add_custom_target(covreport
      DEPENDS multipass_tests
      WORKING_DIRECTORY ${CMAKE_BUILD_DIR}
      COMMAND ${LCOV} --directory . --zerocounters
      COMMAND ${CMAKE_COMMAND} --build ${CMAKE_BINARY_DIR} --target test
      COMMAND ${LCOV} --directory . --capture --output-file coverage.info ${LCOV_IGNORE_ERRORS}
      COMMAND ${LCOV}
        --remove coverage.info
        '/usr/*'
        ${CMAKE_SOURCE_DIR}'/3rd-party/*'
        ${CMAKE_SOURCE_DIR}'/tests/*'
        ${CMAKE_BINARY_DIR}'/*'
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

include(packaging/cpack.cmake OPTIONAL)
