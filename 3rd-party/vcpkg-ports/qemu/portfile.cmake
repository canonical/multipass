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

if(VCPKG_TARGET_IS_OSX)
    vcpkg_host_architecture(HOST_ARCH)
    if(HOST_ARCH STREQUAL "arm64")
        set(QEMU_ARCH aarch64)
    else()
        set(QEMU_ARCH x86_64)
    endif()

    set(EXTRA_ENV "")
    if(DEFINED ENV{MACOSX_DEPLOYMENT_TARGET})
        set(EXTRA_ENV "MACOSX_DEPLOYMENT_TARGET=$ENV{MACOSX_DEPLOYMENT_TARGET}")
    endif()

    set(HVF_FLAG "--enable-hvf")
else()
    set(QEMU_ARCH x86_64)
    set(EXTRA_ENV "")
    set(HVF_FLAG "")
endif()

set(BUILD_DIR "${CURRENT_BUILDTREES_DIR}/${TARGET_TRIPLET}-rel")
file(MAKE_DIRECTORY "${BUILD_DIR}")

message(STATUS "Configuring QEMU")
vcpkg_execute_required_process(
    COMMAND "${SOURCE_PATH}/configure"
        "--prefix=${CURRENT_PACKAGES_DIR}"
        "--firmwarepath=Resources/qemu/"
        "--ninja=${NINJA}"
        "--target-list=${QEMU_ARCH}-softmmu"
        ${HVF_FLAG}
        "--enable-virtfs"
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
    WORKING_DIRECTORY "${BUILD_DIR}"
    LOGNAME configure-${TARGET_TRIPLET}
)

message(STATUS "Building QEMU")
vcpkg_execute_required_process(
    COMMAND ${CMAKE_COMMAND} -E env ${EXTRA_ENV} "${NINJA}"
    WORKING_DIRECTORY "${BUILD_DIR}"
    LOGNAME build-${TARGET_TRIPLET}
)

message(STATUS "Installing QEMU")
vcpkg_execute_required_process(
    COMMAND "${NINJA}" install
    WORKING_DIRECTORY "${BUILD_DIR}"
    LOGNAME install-${TARGET_TRIPLET}
)

# Keep only the binaries and firmware files Multipass needs
set(TOOLS
    "${CURRENT_PACKAGES_DIR}/bin/qemu-img"
    "${CURRENT_PACKAGES_DIR}/bin/qemu-system-${QEMU_ARCH}"
)
foreach(tool IN LISTS TOOLS)
    if(NOT EXISTS "${tool}")
        message(FATAL_ERROR "Expected QEMU binary not found: ${tool}")
    endif()
endforeach()

set(FIRMWARE
    "edk2-${QEMU_ARCH}-code.fd"
    "efi-virtio.rom"
    "vgabios-stdvga.bin"
)

# Move firmware to expected layout
file(MAKE_DIRECTORY "${CURRENT_PACKAGES_DIR}/Resources/qemu")
foreach(fw IN LISTS FIRMWARE)
    file(RENAME
        "${CURRENT_PACKAGES_DIR}/share/qemu/${fw}"
        "${CURRENT_PACKAGES_DIR}/Resources/qemu/${fw}"
    )
endforeach()

# Clean up everything Multipass doesn't need
file(REMOVE_RECURSE
    "${CURRENT_PACKAGES_DIR}/debug"
    "${CURRENT_PACKAGES_DIR}/share/qemu"
    "${CURRENT_PACKAGES_DIR}/include"
    "${CURRENT_PACKAGES_DIR}/lib"
    "${CURRENT_PACKAGES_DIR}/libexec"
    "${CURRENT_PACKAGES_DIR}/var"
)

# vcpkg requires a copyright file
vcpkg_install_copyright(FILE_LIST "${SOURCE_PATH}/COPYING")