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
# Authored by: Antoni Bertolin Monferrer <antoni.monferrer@canonical.com>

# Here the rust build depends on the C++ side of the rust_logger bridge because
# the rust_logger crate is compiled within this crate's static library.
# Because we did not build it, we specify the dependency like:
#list(APPEND ${crate_name}_link_libs "$<LINK_ONLY:rust_logger>")
#Add necessary sources for wrappers (it is advised that the source is named after
# the header in include/multipass/)
#target_sources(${crate_name} PRIVATE "src/name_generator_rs.cpp")
#Link necessary libraries for the exceptions header
#target_link_libraries(${crate_name} PRIVATE fmt::fmt-header-only Qt::Core)

target_sources(petname PRIVATE
  "src/petname_interface.cpp"
  "src/petname_provider.cpp")
