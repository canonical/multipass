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

vcpkg_from_github(
    OUT_SOURCE_PATH SOURCE_PATH
    REPO qemu/QEMU
    REF "v${VERSION}"
    SHA512 f0220682d64a439227c685e849c63710bf346303606dcf9b2b770376f407219d789564b4814cacb31bee345ec8ecf497063c79a4af4d01ea5f1610aad7b731da
    HEAD_REF master
    PATCHES
        multipass-patches/0001-add-disable-tests-option.patch
        multipass-patches/0002-9p-uid-gid-mapping-support.patch
        multipass-patches/0003-revert-old-highmem-off-behavior.patch
        multipass-patches/0004-zero-initialize-vmnet-send-pos.patch
        multipass-patches/0005-revert-fix-encodings-for-id-aa64pfr1-el1.patch
)

find_program(NINJA ninja REQUIRED)

# Derive QEMU arch from vcpkg target
if(VCPKG_TARGET_ARCHITECTURE STREQUAL "arm64")
    set(QEMU_ARCH aarch64)
elseif(VCPKG_TARGET_ARCHITECTURE STREQUAL "x64")
    set(QEMU_ARCH x86_64)
else()
    message(FATAL_ERROR "Unsupported architecture: ${VCPKG_TARGET_ARCHITECTURE}")
endif()

# Platform-specific acceleration
if(VCPKG_TARGET_IS_OSX)
    set(ACCEL_FLAG "--enable-hvf")
else()
    set(ACCEL_FLAG "--enable-kvm")
endif()

# Replace the real configure with our configure wrapper so we can strip the unsupported flags.
# This is done to make use of vcpkg_configure_make.
file(RENAME "${SOURCE_PATH}/configure" "${SOURCE_PATH}/configure.real")
file(COPY "${CMAKE_CURRENT_LIST_DIR}/configure-wrapper" DESTINATION "${SOURCE_PATH}")
file(RENAME "${SOURCE_PATH}/configure-wrapper" "${SOURCE_PATH}/configure")
file(CHMOD "${SOURCE_PATH}/configure" PERMISSIONS OWNER_READ OWNER_WRITE OWNER_EXECUTE)

# Ensure the QEMU build process uses the vcpkg's pkgconfig files
set(ENV{PKG_CONFIG_PATH} "${CURRENT_INSTALLED_DIR}/lib/pkgconfig")
set(ENV{PKG_CONFIG_LIBDIR} "${CURRENT_INSTALLED_DIR}/lib/pkgconfig")

if("system" IN_LIST FEATURES)
    set(QEMU_TARGET_LIST "${QEMU_ARCH}-softmmu")
else()
    set(QEMU_TARGET_LIST "")
endif()

message(STATUS "FEATURES: ${FEATURES}")

# vcpkg_configure_make provides "clean" build environment for the configure process.
# This way, configure won't use anything from the system accidentally.
vcpkg_configure_make(
    SOURCE_PATH "${SOURCE_PATH}"
    DISABLE_VERBOSE_FLAGS
    OPTIONS
        "--ninja=${NINJA}"
        "--target-list=${QEMU_TARGET_LIST}"
        ${ACCEL_FLAG}
        "--enable-virtfs"
        "--extra-cflags=-I${CURRENT_INSTALLED_DIR}/include"
        "--extra-ldflags=-L${CURRENT_INSTALLED_DIR}/lib"
        "--disable-bochs"
        "--disable-cloop"
        "--disable-docs"
        "--disable-guest-agent"
        "--disable-parallels"
        "--disable-qed"
        "--disable-libiscsi"
        "--disable-vnc"
        "--disable-xen"
        "--disable-dmg"
        "--disable-replication"
        "--disable-snappy"
        "--disable-lzo"
        "--disable-vvfat"
        "--disable-curl"
        "--disable-tests"
        "--disable-nettle"
        "--disable-libusb"
        "--disable-bzip2"
        "--disable-gcrypt"
        "--disable-gnutls"
        "--disable-libvduse"
        "--disable-vduse-blk-export"
        "--disable-dbus-display"
        "--disable-xkbcommon"
        "--disable-gtk"
        "--disable-opengl"
        "--disable-libudev"
        "--disable-af-xdp"
        "--disable-rust"
        "--disable-slirp"
)

vcpkg_build_make()
vcpkg_install_make()

# Verify expected binaries exist
set(TOOLS
    "${CURRENT_PACKAGES_DIR}/bin/qemu-img"
    "${CURRENT_PACKAGES_DIR}/bin/qemu-system-${QEMU_ARCH}"
)
foreach(tool IN LISTS TOOLS)
    if(NOT EXISTS "${tool}")
        message(FATAL_ERROR "Expected QEMU binary not found: ${tool}")
    endif()
endforeach()

# Arch-specific firmware
if(QEMU_ARCH STREQUAL "aarch64")
    set(FIRMWARE
        "edk2-aarch64-code.fd"
        "efi-virtio.rom"
    )
else()
    set(FIRMWARE
        "edk2-x86_64-code.fd"
        "efi-virtio.rom"
        "vgabios-stdvga.bin"
    )
endif()

# Move firmware to expected layout
file(MAKE_DIRECTORY "${CURRENT_PACKAGES_DIR}/Resources/qemu")
foreach(fw IN LISTS FIRMWARE)
    if(EXISTS "${CURRENT_PACKAGES_DIR}/share/qemu/${fw}")
        file(RENAME
            "${CURRENT_PACKAGES_DIR}/share/qemu/${fw}"
            "${CURRENT_PACKAGES_DIR}/Resources/qemu/${fw}"
        )
    else()
        message(WARNING "Firmware file not found: ${fw}")
    endif()
endforeach()

file(COPY "${CMAKE_CURRENT_LIST_DIR}/OVMF.fd" DESTINATION "${CURRENT_PACKAGES_DIR}/Resources/qemu")
file(COPY "${SOURCE_PATH}/pc-bios/kvmvapic.bin" DESTINATION "${CURRENT_PACKAGES_DIR}/Resources/qemu")

# On aarch64, also provide the QEMU_EFI.fd alias
if(QEMU_ARCH STREQUAL "aarch64")
    file(CREATE_LINK
        "${CURRENT_PACKAGES_DIR}/Resources/qemu/edk2-aarch64-code.fd"
        "${CURRENT_PACKAGES_DIR}/Resources/qemu/QEMU_EFI.fd"
        COPY_ON_ERROR
    )
endif()

# Clean up everything Multipass doesn't need
file(REMOVE_RECURSE
    "${CURRENT_PACKAGES_DIR}/debug"
    "${CURRENT_PACKAGES_DIR}/share/qemu"
    "${CURRENT_PACKAGES_DIR}/include"
    "${CURRENT_PACKAGES_DIR}/lib"
    "${CURRENT_PACKAGES_DIR}/libexec"
    "${CURRENT_PACKAGES_DIR}/var"
)

set(VCPKG_POLICY_EMPTY_INCLUDE_FOLDER enabled)

vcpkg_install_copyright(FILE_LIST "${SOURCE_PATH}/COPYING")