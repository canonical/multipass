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

determine_version_components(${MULTIPASS_VERSION} MULTIPASS_SEMANTIC_VERSION MULTIPASS_BUILD_NUMBER)
message(STATUS "Setting semantic version to: ${MULTIPASS_SEMANTIC_VERSION}")
message(STATUS "Setting build number to: ${MULTIPASS_BUILD_NUMBER}")

add_library(dart_ffi SHARED ffi/dart_ffi.cpp)

add_custom_command(TARGET dart_ffi POST_BUILD
    COMMAND ${STRIP_COMMAND} "$<TARGET_FILE:dart_ffi>"
    COMMENT "Stripping symbols from dart_ffi..."
)

target_link_libraries(dart_ffi
  petname
  client_common
  client_platform
)

install(TARGETS dart_ffi LIBRARY COMPONENT multipass)

if(${CMAKE_HOST_SYSTEM_PROCESSOR} MATCHES "x86_64" OR ${CMAKE_HOST_SYSTEM_PROCESSOR} MATCHES "AMD64")
  set(MULTIPASS_GUI_ARCH x64)
else()
  set(MULTIPASS_GUI_ARCH arm64)
endif()

if(UNIX)
  if(APPLE)
    include("CMakeLists.txt.macos")
  else() # Linux
    include("CMakeLists.txt.linux")
  endif()
else() # Windows
  include("CMakeLists.txt.windows")
endif()
