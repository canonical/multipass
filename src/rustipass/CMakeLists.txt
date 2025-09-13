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

# Import the rustipass crate using Corrosion
corrosion_import_crate(MANIFEST_PATH ${RUSTIPASS_CRATE_DIR}/Cargo.toml)

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

# Create an interface library that wraps the Rust library and cxx bindings
add_library(rustipass_lib INTERFACE)

# Link the Rust static library (Corrosion automatically handles the target naming)
target_link_libraries(rustipass_lib INTERFACE rustipass rustipass_cxx)

# On Windows, link required system libraries for Rust std
if(WIN32)
  target_link_libraries(rustipass_lib INTERFACE ntdll ws2_32 userenv bcrypt)
endif()

# Create compatibility alias for petname (backward compatibility)
add_library(petname ALIAS rustipass_lib)
