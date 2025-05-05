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
#

function(add_target TARGET_NAME)
  add_library(${TARGET_NAME} STATIC
    console.cpp
    terminal.cpp)

  if(MSVC)
    target_sources(${TARGET_NAME} PRIVATE
      windows_console.cpp
      windows_terminal.cpp)
    qt6_disable_unicode_defines(${TARGET_NAME})
  else()
    target_sources(${TARGET_NAME} PRIVATE
      unix_console.cpp
      unix_terminal.cpp)
  endif()

  target_link_libraries(${TARGET_NAME}
    client_platform
    platform
    libssh)
endfunction()

add_target(console)
if(MULTIPASS_ENABLE_TESTS)
  add_target(console_test)
endif()
