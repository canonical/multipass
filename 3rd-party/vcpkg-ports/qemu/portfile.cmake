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
        multipass-patches/0001-9p-uid-gid-mapping-support.patch
        multipass-patches/0002-revert-old-highmem-off-behavior.patch
        multipass-patches/0003-zero-initialize-vmnet-send-pos.patch
        multipass-patches/0004-revert-fix-encodings-for-id-aa64pfr1-el1.patch
)

if(VCPKG_TARGET_ARCHITECTURE STREQUAL "arm64")
    set(QEMU_ARCH aarch64)
elseif(VCPKG_TARGET_ARCHITECTURE STREQUAL "x64")
    set(QEMU_ARCH x86_64)
elseif(VCPKG_TARGET_ARCHITECTURE STREQUAL "ppc64le")
    set(QEMU_ARCH ppc64)
elseif(VCPKG_TARGET_ARCHITECTURE STREQUAL "s390x")
    set(QEMU_ARCH s390x)
else()
    message(FATAL_ERROR "Unsupported architecture: ${VCPKG_TARGET_ARCHITECTURE}")
endif()

file(WRITE "${SOURCE_PATH}/tests/meson.build" "")

if("system" IN_LIST FEATURES)
    set(QEMU_TARGET_LIST "${QEMU_ARCH}-softmmu")
else()
    set(QEMU_TARGET_LIST "")
endif()

set(QEMU_COMMON_OPTIONS
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
    "--disable-slirp"
    "--disable-curses"
)

if(VCPKG_TARGET_IS_WINDOWS)
    ###########################################################################
    # Windows: QEMU requires GCC/MinGW. Bootstrap a full MSYS2 environment.
    #
    # NOTE: MinGW packages are fetched via pacman and are not version-pinned.
    # Builds are not fully hermetic on Windows. The MSYS2 base tarball is
    # pinned to limit variance. pacman -S and -Sy require network access,
    # which is atypical for vcpkg ports.
    ###########################################################################

    if(NOT VCPKG_TARGET_ARCHITECTURE STREQUAL "x64")
        message(FATAL_ERROR "Windows QEMU build currently only supports x64")
    endif()

    function(to_msys_path WIN_PATH OUT_VAR)
        string(REPLACE "\\" "/" _p "${WIN_PATH}")
        string(REGEX MATCH "^([A-Za-z]):(.*)" _match "${_p}")
        if(_match)
            string(TOLOWER "${CMAKE_MATCH_1}" _drive)
            set(_p "/${_drive}${CMAKE_MATCH_2}")
        endif()
        set(${OUT_VAR} "${_p}" PARENT_SCOPE)
    endfunction()

    vcpkg_acquire_msys(MSYS_ROOT)

    set(MSYS2_DIR "${CURRENT_BUILDTREES_DIR}/msys2-full/msys64")
    set(MSYS2_BASH "${MSYS2_DIR}/usr/bin/bash.exe")
    set(MINGW_SHELL "${MSYS2_BASH}" -lc)
    set(MINGW_ENV "export MSYSTEM=MINGW64 && source /etc/profile")
    set(MSYS2_SETUP_STAMP "${MSYS2_DIR}/.qemu-vcpkg-setup-complete")

    vcpkg_download_distfile(MSYS2_ARCHIVE
        URLS "https://github.com/msys2/msys2-installer/releases/download/2025-02-21/msys2-base-x86_64-20250221.tar.zst"
        FILENAME "msys2-base-x86_64-20250221.tar.zst"
        SHA512 34267856d189ba21de9aba265c866dd0590a078784193161467387aa9f4f9e97c7a0119788955e95d54d4ab2346869852f8e35e006eaec80bf452bad51b213fa
    )

    if(NOT EXISTS "${MSYS2_DIR}/usr/bin/pacman.exe")
        file(MAKE_DIRECTORY "${CURRENT_BUILDTREES_DIR}/msys2-full")
        vcpkg_execute_required_process(
            COMMAND "${MSYS_ROOT}/usr/bin/bash.exe" -lc "cd '${CURRENT_BUILDTREES_DIR}/msys2-full' && tar xf '${MSYS2_ARCHIVE}'"
            WORKING_DIRECTORY "${CURRENT_BUILDTREES_DIR}/msys2-full"
            LOGNAME extract-msys2-${TARGET_TRIPLET}
        )
    endif()

    set(MINGW_PACKAGES
        mingw-w64-x86_64-gcc
        mingw-w64-x86_64-glib2
        mingw-w64-x86_64-pixman
        mingw-w64-x86_64-zstd
        mingw-w64-x86_64-python
        mingw-w64-x86_64-ninja
        mingw-w64-x86_64-pkgconf
    )
    list(JOIN MINGW_PACKAGES " " MINGW_PACKAGES_STRING)

    to_msys_path("${SOURCE_PATH}" SOURCE_PATH_UNIX)
    to_msys_path("${CURRENT_PACKAGES_DIR}" INSTALL_PREFIX_UNIX)
    set(BUILD_DIR "${CURRENT_BUILDTREES_DIR}/${TARGET_TRIPLET}-rel")
    file(MAKE_DIRECTORY "${BUILD_DIR}")
    to_msys_path("${BUILD_DIR}" BUILD_DIR_UNIX)
    list(JOIN QEMU_COMMON_OPTIONS " " COMMON_OPTIONS_STRING)

     if(NOT EXISTS "${MSYS2_SETUP_STAMP}")
        # Initial shell launch to let MSYS2 set up its environment
        vcpkg_execute_required_process(
            COMMAND ${MINGW_SHELL} "true"
            WORKING_DIRECTORY "${MSYS2_DIR}"
            LOGNAME msys2-init-${TARGET_TRIPLET}
        )
        vcpkg_execute_required_process(
            COMMAND ${MINGW_SHELL} "pacman -Rdd --noconfirm mingw-w64-x86_64-pkg-config || true && pacman -Sy --noconfirm && pacman -S --noconfirm --needed ${MINGW_PACKAGES_STRING}"
            WORKING_DIRECTORY "${MSYS2_DIR}"
            LOGNAME msys2-packages-${TARGET_TRIPLET}
        )
        vcpkg_execute_required_process(
            COMMAND ${MINGW_SHELL} "${MINGW_ENV} && rm -f /mingw64/lib/python3.*/EXTERNALLY-MANAGED && python -m ensurepip && python -m pip install distlib '${SOURCE_PATH_UNIX}'/python/wheels/*.whl"
            WORKING_DIRECTORY "${MSYS2_DIR}"
            LOGNAME msys2-python-deps-${TARGET_TRIPLET}
        )
        file(TOUCH "${MSYS2_SETUP_STAMP}")
    endif()

    # Configure
    vcpkg_execute_required_process(
        COMMAND ${MINGW_SHELL} "${MINGW_ENV} && cd '${BUILD_DIR_UNIX}' && '${SOURCE_PATH_UNIX}/configure' --static --enable-tools --target-list='${QEMU_TARGET_LIST}' --prefix='${INSTALL_PREFIX_UNIX}' --extra-cflags='-UNDEBUG' --extra-ldflags='-liconv' ${COMMON_OPTIONS_STRING}"
        WORKING_DIRECTORY "${BUILD_DIR}"
        LOGNAME configure-${TARGET_TRIPLET}
    )

    # Build
    vcpkg_execute_required_process(
        COMMAND ${MINGW_SHELL} "${MINGW_ENV} && cd '${BUILD_DIR_UNIX}' && ninja -j${VCPKG_CONCURRENCY}"
        WORKING_DIRECTORY "${BUILD_DIR}"
        LOGNAME build-${TARGET_TRIPLET}
    )

    # Install
    vcpkg_execute_required_process(
        COMMAND ${MINGW_SHELL} "${MINGW_ENV} && cd '${BUILD_DIR_UNIX}' && ninja install"
        WORKING_DIRECTORY "${BUILD_DIR}"
        LOGNAME install-${TARGET_TRIPLET}
    )

    # ninja install puts binaries in the prefix root; move to bin/
    file(MAKE_DIRECTORY "${CURRENT_PACKAGES_DIR}/bin")
    file(GLOB QEMU_EXES "${CURRENT_PACKAGES_DIR}/*.exe")
    foreach(exe IN LISTS QEMU_EXES)
        get_filename_component(name "${exe}" NAME)
        file(RENAME "${exe}" "${CURRENT_PACKAGES_DIR}/bin/${name}")
    endforeach()

else()
    ###########################################################################
    # Linux / macOS
    ###########################################################################

    find_program(NINJA ninja REQUIRED)

    if(VCPKG_TARGET_IS_OSX)
        set(ACCEL_FLAG "--enable-hvf")
    else()
        set(ACCEL_FLAG "--enable-kvm")
    endif()

    # Replace configure with our wrapper to strip unsupported flags
    # injected by vcpkg_configure_make.
    file(RENAME "${SOURCE_PATH}/configure" "${SOURCE_PATH}/configure.real")
    file(COPY "${CMAKE_CURRENT_LIST_DIR}/configure-wrapper" DESTINATION "${SOURCE_PATH}")
    file(RENAME "${SOURCE_PATH}/configure-wrapper" "${SOURCE_PATH}/configure")
    file(CHMOD "${SOURCE_PATH}/configure" PERMISSIONS OWNER_READ OWNER_WRITE OWNER_EXECUTE)

    set(ENV{PKG_CONFIG_PATH} "${CURRENT_INSTALLED_DIR}/lib/pkgconfig")
    set(ENV{PKG_CONFIG_LIBDIR} "${CURRENT_INSTALLED_DIR}/lib/pkgconfig")

    vcpkg_configure_make(
        SOURCE_PATH "${SOURCE_PATH}"
        DISABLE_VERBOSE_FLAGS
        OPTIONS
            "--ninja=${NINJA}"
            "--target-list=${QEMU_TARGET_LIST}"
            ${ACCEL_FLAG}
            "--enable-virtfs"
            "--extra-cflags=-I${CURRENT_INSTALLED_DIR}/include -UNDEBUG"
            "--extra-ldflags=-L${CURRENT_INSTALLED_DIR}/lib"
            ${QEMU_COMMON_OPTIONS}
    )

    vcpkg_build_make()
    vcpkg_install_make()
endif()

# Verify expected binaries
set(TOOLS "${CURRENT_PACKAGES_DIR}/bin/qemu-img${VCPKG_TARGET_EXECUTABLE_SUFFIX}")
if("system" IN_LIST FEATURES)
    list(APPEND TOOLS "${CURRENT_PACKAGES_DIR}/bin/qemu-system-${QEMU_ARCH}${VCPKG_TARGET_EXECUTABLE_SUFFIX}")
endif()
foreach(tool IN LISTS TOOLS)
    if(NOT EXISTS "${tool}")
        message(FATAL_ERROR "Expected QEMU binary not found: ${tool}")
    endif()
endforeach()

# Firmware
if(QEMU_ARCH STREQUAL "aarch64")
    set(FIRMWARE "edk2-aarch64-code.fd")
elseif(QEMU_ARCH STREQUAL "x86_64")
    set(FIRMWARE "edk2-x86_64-code.fd")
elseif(QEMU_ARCH STREQUAL "ppc64")
    set(FIRMWARE "slof.bin")
elseif(QEMU_ARCH STREQUAL "s390x")
    set(FIRMWARE "s390-ccw.img")
endif()

file(MAKE_DIRECTORY "${CURRENT_PACKAGES_DIR}/Resources/qemu")
foreach(fw IN LISTS FIRMWARE)
    if(EXISTS "${CURRENT_PACKAGES_DIR}/share/qemu/${fw}")
        file(RENAME
            "${CURRENT_PACKAGES_DIR}/share/qemu/${fw}"
            "${CURRENT_PACKAGES_DIR}/Resources/qemu/${fw}")
    else()
        message(WARNING "Firmware file not found: ${fw}")
    endif()
endforeach()

file(COPY "${CMAKE_CURRENT_LIST_DIR}/OVMF.fd" DESTINATION "${CURRENT_PACKAGES_DIR}/Resources/qemu")

if("system" IN_LIST FEATURES)
    file(COPY "${SOURCE_PATH}/pc-bios/kvmvapic.bin" DESTINATION "${CURRENT_PACKAGES_DIR}/Resources/qemu")
    file(COPY "${SOURCE_PATH}/pc-bios/vgabios-stdvga.bin" DESTINATION "${CURRENT_PACKAGES_DIR}/Resources/qemu")
    file(COPY "${SOURCE_PATH}/pc-bios/efi-virtio.rom" DESTINATION "${CURRENT_PACKAGES_DIR}/Resources/qemu")
endif()

if(QEMU_ARCH STREQUAL "aarch64")
    file(CREATE_LINK
        "${CURRENT_PACKAGES_DIR}/Resources/qemu/edk2-aarch64-code.fd"
        "${CURRENT_PACKAGES_DIR}/Resources/qemu/QEMU_EFI.fd"
        COPY_ON_ERROR)
endif()

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