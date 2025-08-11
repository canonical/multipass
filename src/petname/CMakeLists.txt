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
# Authored by: Alberto Aguirre <alberto.aguirre@canonical.com>

# Build the Rust static library and generate CXX bridge headers
if(WIN32)
  set(RUST_LIB_NAME "multipass.lib")
  set(CARGO_PATH_ENV "PATH=%USERPROFILE%\\.cargo\\bin;$ENV{PATH}")
else()
  set(RUST_LIB_NAME "libmultipass.a")
  set(CARGO_PATH_ENV "PATH=$ENV{HOME}/.cargo/bin:$ENV{PATH}")
endif()

add_custom_command(
  OUTPUT ${CMAKE_CURRENT_BINARY_DIR}/release/${RUST_LIB_NAME} ${CMAKE_CURRENT_BINARY_DIR}/cxxbridge/lib.rs.h ${CMAKE_CURRENT_BINARY_DIR}/cxxbridge/lib.rs.cc
  COMMAND ${CMAKE_COMMAND} -E env "${CARGO_PATH_ENV}" cargo build --release --manifest-path ${CMAKE_SOURCE_DIR}/Cargo.toml --target-dir ${CMAKE_CURRENT_BINARY_DIR}
  DEPENDS ${CMAKE_SOURCE_DIR}/Cargo.toml ${CMAKE_SOURCE_DIR}/src/lib.rs ${CMAKE_SOURCE_DIR}/src/petname/mod.rs
  COMMENT "Building petname Rust library"
  VERBATIM)

# Create a target for the Rust library
add_custom_target(multipass_petname ALL DEPENDS ${CMAKE_CURRENT_BINARY_DIR}/release/${RUST_LIB_NAME} ${CMAKE_CURRENT_BINARY_DIR}/cxxbridge/lib.rs.h ${CMAKE_CURRENT_BINARY_DIR}/cxxbridge/lib.rs.cc)

# Create an interface library that just links the Rust static library
add_library(petname INTERFACE)
target_include_directories(petname INTERFACE ${CMAKE_CURRENT_BINARY_DIR}/cxxbridge)
target_link_libraries(petname INTERFACE ${CMAKE_CURRENT_BINARY_DIR}/release/${RUST_LIB_NAME})

# On Windows, link required system libraries for Rust std
if(WIN32)
  target_link_libraries(petname INTERFACE ntdll userenv ws2_32 bcrypt)
endif()

# Add the Rust library as a dependency for the petname library
add_dependencies(petname multipass_petname)
