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
set(WITH_STATIC_LIB TRUE)
set(WITH_EXAMPLES FALSE)
set(WITH_SERVER TRUE)
set(WITH_SFTP TRUE)
set(WITH_CHACHAPOLY TRUE)

# We'll link with the boringssl implementation in grpc
set(OPENSSL_FOUND TRUE)
# Lie about the version so that we can do our own crypto-compat layer
set(OPENSSL_VERSION "1.1.1")

set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -Wno-implicit-function-declaration -Wno-incompatible-pointer-types -Wno-int-conversion")

set(OPENSSL_INCLUDE_DIR ${CMAKE_CURRENT_SOURCE_DIR}/../grpc/third_party/boringssl/include)
set(LIBSSH_INCLUDE_DIR ${CMAKE_CURRENT_SOURCE_DIR}/libssh/include)

# Needed only because of libssh install target
set(LIB_INSTALL_DIR ${CMAKE_CURRENT_BINARY_DIR})

include_directories(${LIBSSH_INCLUDE_DIR})
include_directories(${OPENSSL_INCLUDE_DIR})
include_directories(${CMAKE_CURRENT_BINARY_DIR}/libssh)

# libssh source needs the config.h header to be generated
include(libssh/ConfigureChecks.cmake)
configure_file(libssh/config.h.cmake ${CMAKE_CURRENT_BINARY_DIR}/libssh/config.h)

# We bypass the main CMake file to avoid various unneded checks
add_subdirectory(libssh/src EXCLUDE_FROM_ALL)

add_library(ssh-boringssl-compat STATIC
  ssh-boringssl-compat.c)

add_library(libssh INTERFACE)

target_include_directories(libssh INTERFACE
  ${LIBSSH_INCLUDE_DIR})

target_link_libraries(libssh INTERFACE
  ssh_static
  crypto
  decrepit
  ssh-boringssl-compat)
