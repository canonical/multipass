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

# Build the petname Rust crate as a standalone library
if(WIN32)
  set(RUST_LIB_NAME "multipass_petname.lib")
  set(CARGO_PATH_ENV "PATH=%USERPROFILE%\\.cargo\\bin;$ENV{PATH}")
else()
  set(RUST_LIB_NAME "libmultipass_petname.a")
  set(CARGO_PATH_ENV "PATH=$ENV{HOME}/.cargo/bin:$ENV{PATH}")
endif()

# Set the target directory for this specific crate
set(PETNAME_TARGET_DIR ${CMAKE_CURRENT_BINARY_DIR}/petname-target)
set(PETNAME_CRATE_DIR ${CMAKE_SOURCE_DIR}/crates/petname)

add_custom_command(
  OUTPUT
    ${PETNAME_TARGET_DIR}/release/${RUST_LIB_NAME}
    ${PETNAME_TARGET_DIR}/cxxbridge/src/lib.rs.h
    ${PETNAME_TARGET_DIR}/cxxbridge/src/lib.rs.cc
  COMMAND ${CMAKE_COMMAND} -E env "${CARGO_PATH_ENV}"
    cargo build --release --manifest-path ${PETNAME_CRATE_DIR}/Cargo.toml --target-dir ${PETNAME_TARGET_DIR}
  DEPENDS
    ${PETNAME_CRATE_DIR}/Cargo.toml
    ${PETNAME_CRATE_DIR}/src/lib.rs
    ${PETNAME_CRATE_DIR}/build.rs
    ${CMAKE_SOURCE_DIR}/src/petname/adjectives.txt
    ${CMAKE_SOURCE_DIR}/src/petname/adverbs.txt
    ${CMAKE_SOURCE_DIR}/src/petname/names.txt
  COMMENT "Building petname Rust crate"
  VERBATIM)

# Create a target for the petname Rust library
add_custom_target(multipass_petname_crate ALL
  DEPENDS
    ${PETNAME_TARGET_DIR}/release/${RUST_LIB_NAME}
    ${PETNAME_TARGET_DIR}/cxxbridge/src/lib.rs.h
    ${PETNAME_TARGET_DIR}/cxxbridge/src/lib.rs.cc)

# Create an interface library that links the petname Rust static library
add_library(petname INTERFACE)
target_include_directories(petname INTERFACE
  ${PETNAME_TARGET_DIR}/cxxbridge
  ${PETNAME_TARGET_DIR}/cxxbridge/rust)
target_link_libraries(petname INTERFACE ${PETNAME_TARGET_DIR}/release/${RUST_LIB_NAME})

# On Windows, link required system libraries for Rust std
if(WIN32)
  target_link_libraries(petname INTERFACE ntdll ws2_32 userenv bcrypt)
endif()

# Add the Rust library as a dependency for the petname library
add_dependencies(petname multipass_petname_crate)
