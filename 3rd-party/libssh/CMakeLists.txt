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

set(WITH_ZLIB FALSE)
set(WITH_EXAMPLES FALSE)
set(WITH_SERVER TRUE)
set(WITH_SFTP TRUE)

# We'll link with the boringssl implementation in grpc
set(OPENSSL_FOUND TRUE)
# Lie about the version so that we can do our own crypto-compat layer
set(OPENSSL_VERSION "1.1.1")

set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -Wno-implicit-function-declaration -Wno-incompatible-pointer-types -Wno-int-conversion")

set(OPENSSL_INCLUDE_DIR ${CMAKE_CURRENT_SOURCE_DIR}/../grpc/third_party/boringssl/include)
set(LIBSSH_INCLUDE_DIR ${CMAKE_CURRENT_SOURCE_DIR}/libssh/include)

# Needed only because of libssh install target
set(LIB_INSTALL_DIR lib)
set(BIN_INSTALL_DIR bin)

include_directories(${LIBSSH_INCLUDE_DIR})
include_directories(${OPENSSL_INCLUDE_DIR})
include_directories(${CMAKE_CURRENT_BINARY_DIR}/libssh)

# libssh source needs the config.h header to be generated
include(libssh/ConfigureChecks.cmake)
configure_file(libssh/config.h.cmake ${CMAKE_CURRENT_BINARY_DIR}/libssh/config.h)

# Must be set after configure checks as crypto has not been built yet
# but will be included in the final libssh shared library
set(OPENSSL_CRYPTO_LIBRARY crypto decrepit ssh-boringssl-compat)

message(STATUS "Checking for 'libssh' version")

# Since the main CMake file is bypassed, we have to search that CMake file to determine the proper
# shared library versions
file(STRINGS libssh/CMakeLists.txt LIBRARY_VERSION REGEX "^set\\(LIBRARY_VERSION")
file(STRINGS libssh/CMakeLists.txt LIBRARY_SOVERSION REGEX "^set\\(LIBRARY_SOVERSION")

string(REGEX REPLACE "^set\\(LIBRARY_VERSION \"(.*)\"\\)$" "\\1"
       LIBRARY_VERSION "${LIBRARY_VERSION}")
if (NOT LIBRARY_VERSION)
    message(FATAL_ERROR "unable to find libssh library version")
endif()

string(REGEX REPLACE "^set\\(LIBRARY_SOVERSION \"(.*)\"\\)$" "\\1"
       LIBRARY_SOVERSION "${LIBRARY_SOVERSION}")
if (NOT LIBRARY_SOVERSION)
    message(FATAL_ERROR "unable to find libssh library soversion")
endif()

message(STATUS "  Found ${LIBRARY_VERSION}, ${LIBRARY_SOVERSION}")

# We bypass the main CMake file to avoid various package checks which are satisfied manually
# through the configuration above.
add_subdirectory(libssh/src)

add_library(ssh-boringssl-compat STATIC
  ssh-boringssl-compat.c)

add_library(libssh INTERFACE)

target_include_directories(libssh INTERFACE
  ${LIBSSH_INCLUDE_DIR})

target_link_libraries(libssh INTERFACE
  ssh_shared)
