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

## Ensure build deps are available
pkg_check_modules(GLIB REQUIRED glib-2.0)
pkg_check_modules(PIXMAN REQUIRED pixman-1)

cmake_policy(SET CMP0097 NEW) # Use new empty GIT_SUBMODULES behavior

include(FetchContent)
set(FETCHCONTENT_QUIET FALSE)
set(QEMU_SOURCE_DIR "${CMAKE_BINARY_DIR}/3rd-party/qemu")

FetchContent_Declare(qemu
  SOURCE_DIR ${QEMU_SOURCE_DIR}
  GIT_REPOSITORY https://github.com/canonical/qemu.git
  GIT_TAG v8.0.0+9p-uid-gid-map
  GIT_SHALLOW TRUE
  GIT_SUBMODULES ""
  GIT_PROGRESS TRUE
)
FetchContent_MakeAvailable(qemu)

ExternalProject_Add(qemu
  SOURCE_DIR ${QEMU_SOURCE_DIR}
  CONFIGURE_COMMAND test -f <BINARY_DIR>/config-host.mak ||
    <SOURCE_DIR>/configure
      --prefix=${CMAKE_BINARY_DIR}
      --firmwarepath=${CMAKE_BINARY_DIR}/Resources/qemu/
      --ninja=ninja
      --target-list=${HOST_ARCH}-softmmu
      --enable-hvf
      --enable-virtfs
      --disable-bochs
      --disable-cloop
      --disable-docs
      --disable-guest-agent
      --disable-parallels
      --disable-qed
      --disable-libiscsi
      --disable-vnc
      --disable-xen
      --disable-dmg
      --disable-replication
      --disable-hax
      --disable-snappy
      --disable-lzo
      --disable-live-block-migration
      --disable-vvfat
      --disable-curl
      --disable-tests
      --disable-nettle
      --disable-libusb
      --disable-bzip2
      --disable-gcrypt
      --disable-gnutls
      --disable-libvduse
      --disable-vduse-blk-export
  BUILD_COMMAND ninja
  TEST_COMMAND ""
)

install(PROGRAMS ${CMAKE_BINARY_DIR}/bin/qemu-img ${CMAKE_BINARY_DIR}/bin/qemu-system-${HOST_ARCH}
        DESTINATION bin
        COMPONENT multipassd
)

install(FILES ${CMAKE_BINARY_DIR}/share/qemu/edk2-${HOST_ARCH}-code.fd
              ${CMAKE_BINARY_DIR}/share/qemu/efi-virtio.rom
              ${CMAKE_BINARY_DIR}/share/qemu/vgabios-stdvga.bin
        DESTINATION Resources/qemu
        COMPONENT multipassd
)
