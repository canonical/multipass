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

# Set the crate directory (Corrosion is already available from root CMakeLists.txt)
set(RUSTIPASS_CRATE_DIR ${CMAKE_SOURCE_DIR}/crates/rustipass)

# Configure Rust build profile based on CMAKE_BUILD_TYPE
if(CMAKE_BUILD_TYPE STREQUAL "Release")
    set(RUST_PROFILE "release")
else()
    set(RUST_PROFILE "dev")
endif()

# Import the rustipass crate using Corrosion with the appropriate profile
corrosion_import_crate(MANIFEST_PATH ${RUSTIPASS_CRATE_DIR}/Cargo.toml PROFILE ${RUST_PROFILE})

# Add cxxbridge support for C++ interop
corrosion_add_cxxbridge(rustipass_cxx
    CRATE rustipass
    REGEN_TARGET rustipass_cxx_regen
    FILES lib.rs
)

# Fix macOS build issue: Apple Clang treats '$' in identifiers as non-standard extension
# cxxbridge generates identifiers with '$' from Rust namespaces (multipass::petname -> multipass$petname$...)
if(APPLE)
    target_compile_options(rustipass_cxx PRIVATE -Wno-dollar-in-identifier-extension)
endif()

# Add C++ wrapper for the Rust petname generator
add_library(rustipass_cpp STATIC
    rust_petname_generator.cpp
)

target_include_directories(rustipass_cpp
    PUBLIC
        ${CMAKE_SOURCE_DIR}/include
        ${CMAKE_SOURCE_DIR}/src
    PRIVATE
        ${CMAKE_CURRENT_SOURCE_DIR}
)

target_link_libraries(rustipass_cpp
    PUBLIC rustipass rustipass_cxx
)

# Create an interface library that wraps everything together
add_library(rustipass_lib INTERFACE)

# Link the Rust static library and C++ wrapper
target_link_libraries(rustipass_lib INTERFACE rustipass_cpp rustipass rustipass_cxx)

# On Windows, link required system libraries for Rust std
if(WIN32)
  target_link_libraries(rustipass_lib INTERFACE ntdll ws2_32 userenv bcrypt)
endif()

# Create compatibility alias for petname (backward compatibility)
add_library(petname ALIAS rustipass_lib)
