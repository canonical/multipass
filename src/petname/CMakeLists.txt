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

# Set the crate directory (Corrosion is already available from root CMakeLists.txt)
set(PETNAME_CRATE_DIR ${CMAKE_SOURCE_DIR}/crates/petname)

# Import the petname crate using Corrosion
corrosion_import_crate(MANIFEST_PATH ${PETNAME_CRATE_DIR}/Cargo.toml)

# Add cxxbridge support for C++ interop (experimental feature)
corrosion_add_cxxbridge(petname_cxx
    CRATE multipass_petname
    REGEN_TARGET petname_cxx_regen
    FILES lib.rs
)

# Create an interface library that wraps the Rust library and cxx bindings
add_library(petname INTERFACE)

# Link the Rust static library (Corrosion automatically handles the target naming)
target_link_libraries(petname INTERFACE multipass_petname petname_cxx)

# On Windows, link required system libraries for Rust std
if(WIN32)
  target_link_libraries(petname INTERFACE ntdll ws2_32 userenv bcrypt)
endif()
