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

function(add_target TARGET_NAME)
  if(LINUX)
    add_library(${TARGET_NAME} STATIC
      platform_linux.cpp
      platform_unix.cpp)
    target_link_libraries(${TARGET_NAME} logger_linux)
  elseif(MSVC)
    add_library(${TARGET_NAME} STATIC
      platform_proprietary.cpp
      platform_win.cpp)
    qt6_disable_unicode_defines(${TARGET_NAME})
    target_link_libraries(${TARGET_NAME}
      PUBLIC
      jsoncpp_static
      shared_win
      scope_guard
      wineventlogger
      OpenSSL::applink)
  elseif(APPLE)
    add_library(${TARGET_NAME} STATIC
      platform_osx.cpp
      platform_proprietary.cpp
      platform_unix.cpp)
    target_link_libraries(${TARGET_NAME}
      shared_macos)
  endif()

  foreach(BACKEND IN LISTS MULTIPASS_BACKENDS)
    string(TOUPPER ${BACKEND}_ENABLED DEF)
    target_compile_definitions(${TARGET_NAME} PRIVATE -D${DEF})

    target_link_libraries(${TARGET_NAME} PUBLIC
      ${BACKEND}_backend)
  endforeach()

  include_directories(${TARGET_NAME}
    backends
    ${CMAKE_SOURCE_DIR}/src)

  target_link_libraries(${TARGET_NAME} PUBLIC
    daemon
    settings
    shared
    update)
endfunction()

option(MULTIPASS_ENABLE_SYSLOG "Use syslog for logging instead of journald" OFF)

if(NOT MULTIPASS_ENABLE_SYSLOG)
  add_compile_definitions(MULTIPASS_JOURNALD_ENABLED)
endif()

add_target(platform)
if(MULTIPASS_ENABLE_TESTS)
  add_target(platform_test)
endif()

add_subdirectory(backends)
add_subdirectory(client)
add_subdirectory(console)
add_subdirectory(logger)
add_subdirectory(update)
