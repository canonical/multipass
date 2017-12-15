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

if(cmake_build_type_lower MATCHES "coverage")
  add_compile_options(-ftest-coverage -fprofile-arcs)
  link_libraries(gcov)
endif()

configure_file(
  version.h.in
  ${MULTIPASS_GENERATED_SOURCE_DIR}/multipass/version.h
  @ONLY
)

add_subdirectory(client)
add_subdirectory(daemon)
add_subdirectory(iso)
add_subdirectory(network)
add_subdirectory(petname)
add_subdirectory(platform)
add_subdirectory(rpc)
add_subdirectory(simplestreams)
add_subdirectory(ssh)
add_subdirectory(sshfs_mount)
add_subdirectory(utils)
