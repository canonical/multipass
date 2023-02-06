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

configure_file(
  version.h.in
  ${MULTIPASS_GENERATED_SOURCE_DIR}/multipass/version.h
  @ONLY
)

add_subdirectory(cert)
add_subdirectory(client)
add_subdirectory(daemon)
add_subdirectory(iso)
add_subdirectory(logging)
add_subdirectory(network)
add_subdirectory(petname)
add_subdirectory(platform)
add_subdirectory(process)
add_subdirectory(rpc)
add_subdirectory(settings)
add_subdirectory(simplestreams)
add_subdirectory(ssh)
add_subdirectory(sshfs_mount)
add_subdirectory(utils)
add_subdirectory(blueprint_provider)
add_subdirectory(xz_decoder)
