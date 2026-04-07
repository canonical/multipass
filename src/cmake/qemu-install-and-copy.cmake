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

# This module propagates QEMU binaries to wherever they're needed to exist.

find_program(QEMU_IMG qemu-img
      PATHS "${VCPKG_INSTALLED_DIR}/${VCPKG_TARGET_TRIPLET}/bin"
      NO_DEFAULT_PATH REQUIRED
)

find_program(QEMU_SYSTEM qemu-system-${HOST_ARCH}
    PATHS "${VCPKG_INSTALLED_DIR}/${VCPKG_TARGET_TRIPLET}/bin"
    NO_DEFAULT_PATH REQUIRED
)

add_custom_command(
    OUTPUT "${CMAKE_BINARY_DIR}/bin/qemu-img" "${CMAKE_BINARY_DIR}/bin/qemu-system-${HOST_ARCH}"
    COMMAND ${CMAKE_COMMAND} -E make_directory "${CMAKE_BINARY_DIR}/bin"
    COMMAND ${CMAKE_COMMAND} -E copy "${QEMU_IMG}" "${CMAKE_BINARY_DIR}/bin/"
    COMMAND ${CMAKE_COMMAND} -E copy "${QEMU_SYSTEM}" "${CMAKE_BINARY_DIR}/bin/"
    DEPENDS "${QEMU_IMG}" "${QEMU_SYSTEM}"
)

add_custom_target(qemu ALL DEPENDS
    "${CMAKE_BINARY_DIR}/bin/qemu-img"
    "${CMAKE_BINARY_DIR}/bin/qemu-system-${HOST_ARCH}"
)

install(PROGRAMS "${QEMU_IMG}" "${QEMU_SYSTEM}"
    DESTINATION bin
    COMPONENT multipassd
)