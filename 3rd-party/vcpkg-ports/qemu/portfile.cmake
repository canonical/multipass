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
    "--disable-sdl"
    "--disable-sdl-image"
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
    # The Windows build is deliberately limited to qemu-img (the only QEMU tool
    # Multipass uses on Windows). The system emulator is not built here.
    #
    # NOTE: MinGW packages are fetched via pacman and are not version-pinned.
    # Builds are not fully hermetic on Windows. The MSYS2 base tarball is
    # pinned to limit variance. pacman requires network access, which is
    # atypical for vcpkg ports.
    ###########################################################################

    if(NOT VCPKG_TARGET_ARCHITECTURE STREQUAL "x64")
        message(FATAL_ERROR "Windows QEMU build currently only supports x64")
    endif()

    if("system" IN_LIST FEATURES)
        message(FATAL_ERROR "The 'system' feature (QEMU system emulator) is not supported on Windows; only qemu-img is built")
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

    set(MSYS2_DIR "${CURRENT_BUILDTREES_DIR}/msys2-full/msys64")
    set(MSYS2_BASH "${MSYS2_DIR}/usr/bin/bash.exe")
    set(MINGW_SHELL "${MSYS2_BASH}" -lc)
    set(MSYS2_SETUP_STAMP "${MSYS2_DIR}/.qemu-vcpkg-setup-complete")

    # Select the MinGW64 toolchain for the login shell. With MSYSTEM set in the
    # environment, `bash -lc` sources /etc/profile already configured for
    # MinGW64, so individual commands don't need to export it and re-source.
    set(ENV{MSYSTEM} "MINGW64")

    macro(msys2_exec COMMAND_STRING LOGNAME_SUFFIX)
        vcpkg_execute_required_process(
            COMMAND ${MINGW_SHELL} "${COMMAND_STRING}"
            WORKING_DIRECTORY "${MSYS2_DIR}"
            LOGNAME ${LOGNAME_SUFFIX}-${TARGET_TRIPLET}
        )
    endmacro()

    vcpkg_download_distfile(MSYS2_ARCHIVE
        URLS "https://github.com/msys2/msys2-installer/releases/download/2025-02-21/msys2-base-x86_64-20250221.tar.zst"
        FILENAME "msys2-base-x86_64-20250221.tar.zst"
        SHA512 34267856d189ba21de9aba265c866dd0590a078784193161467387aa9f4f9e97c7a0119788955e95d54d4ab2346869852f8e35e006eaec80bf452bad51b213fa
    )

    # CMake's bundled libarchive handles .tar.zst, so extract directly instead
    # of acquiring a separate MSYS2 just to run `tar`.
    if(NOT EXISTS "${MSYS2_DIR}/usr/bin/pacman.exe")
        file(ARCHIVE_EXTRACT
            INPUT "${MSYS2_ARCHIVE}"
            DESTINATION "${CURRENT_BUILDTREES_DIR}/msys2-full"
        )
    endif()

    # Minimal toolchain/deps to build qemu-img: gcc, glib2 (pulls zlib/iconv),
    # zstd (qcow2 zstd compression), plus the meson/ninja/pkgconf build tools.
    # pixman is intentionally omitted: it is only used by display/SPICE/GTK/VNC,
    # none of which qemu-img links against.
    set(MINGW_PACKAGES
        mingw-w64-x86_64-gcc
        mingw-w64-x86_64-glib2
        mingw-w64-x86_64-zstd
        mingw-w64-x86_64-python
        mingw-w64-x86_64-ninja
        mingw-w64-x86_64-pkgconf
    )
    list(JOIN MINGW_PACKAGES " " MINGW_PACKAGES_STRING)

    to_msys_path("${SOURCE_PATH}" SOURCE_PATH_UNIX)
    set(BUILD_DIR "${CURRENT_BUILDTREES_DIR}/${TARGET_TRIPLET}-rel")
    file(MAKE_DIRECTORY "${BUILD_DIR}")
    to_msys_path("${BUILD_DIR}" BUILD_DIR_UNIX)
    to_msys_path("${CURRENT_PACKAGES_DIR}" INSTALL_PREFIX_UNIX)

    if(NOT EXISTS "${MSYS2_SETUP_STAMP}")
        msys2_exec("true" msys2-init)
        msys2_exec("pacman -Rdd --noconfirm mingw-w64-x86_64-pkg-config || true && pacman -Sy --noconfirm && pacman -S --noconfirm --needed ${MINGW_PACKAGES_STRING}" msys2-packages)
        msys2_exec("rm -f /mingw64/lib/python3.*/EXTERNALLY-MANAGED && python -m ensurepip && python -m pip install distlib '${SOURCE_PATH_UNIX}'/python/wheels/*.whl" msys2-python-deps)
        file(TOUCH "${MSYS2_SETUP_STAMP}")
    endif()

    # --enable-tools is required for the qemu-img target to exist, but we build
    # only that single ninja target (not qemu-io/nbd/storage-daemon/edid) and
    # skip `ninja install` entirely. The binary is copied straight from the
    # build tree. --disable-system avoids building any system emulator; pixman
    # is force-disabled and zstd force-enabled so features don't depend on
    # autodetection of whatever MSYS2 provides. A --prefix is still required:
    # meson validates it at configure time even though we never install.
    msys2_exec("cd '${BUILD_DIR_UNIX}' && '${SOURCE_PATH_UNIX}/configure' --static --enable-tools --disable-system --disable-pixman --enable-zstd --prefix='${INSTALL_PREFIX_UNIX}' --extra-cflags='-UNDEBUG' --extra-ldflags='-liconv'" configure)
    msys2_exec("cd '${BUILD_DIR_UNIX}' && ninja -j${VCPKG_CONCURRENCY} qemu-img.exe" build)

    # Copy just qemu-img.exe from the build tree (no install step ran).
    file(MAKE_DIRECTORY "${CURRENT_PACKAGES_DIR}/bin")
    file(COPY "${BUILD_DIR}/qemu-img.exe" DESTINATION "${CURRENT_PACKAGES_DIR}/bin")

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

# Firmware and UEFI assets are only needed by the system emulator, which is
# never built on Windows (there only qemu-img is produced).
if("system" IN_LIST FEATURES)
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
    if(EXISTS "${CURRENT_PACKAGES_DIR}/share/qemu/${FIRMWARE}")
        file(RENAME
            "${CURRENT_PACKAGES_DIR}/share/qemu/${FIRMWARE}"
            "${CURRENT_PACKAGES_DIR}/Resources/qemu/${FIRMWARE}")
    else()
        message(FATAL_ERROR "Firmware file not found: ${FIRMWARE}")
    endif()

    # Common pc-bios ROMs loaded by the system emulator at runtime.
    file(COPY "${SOURCE_PATH}/pc-bios/kvmvapic.bin" DESTINATION "${CURRENT_PACKAGES_DIR}/Resources/qemu")
    file(COPY "${SOURCE_PATH}/pc-bios/vgabios-stdvga.bin" DESTINATION "${CURRENT_PACKAGES_DIR}/Resources/qemu")
    file(COPY "${SOURCE_PATH}/pc-bios/efi-virtio.rom" DESTINATION "${CURRENT_PACKAGES_DIR}/Resources/qemu")

    if(QEMU_ARCH STREQUAL "x86_64")
        # Ship the legacy 2 MB combined OVMF image so instances suspended by
        # older Multipass releases (which booted with `-bios OVMF.fd`) can still
        # be resumed: resume replays the saved command line verbatim. New VMs use
        # the split edk2 firmware via pflash instead.
        # TODO: drop this once suspended instances from those releases are no
        # longer supported (a couple of releases).
        file(COPY "${CMAKE_CURRENT_LIST_DIR}/vendor/OVMF.fd" DESTINATION "${CURRENT_PACKAGES_DIR}/Resources/qemu")
    elseif(QEMU_ARCH STREQUAL "aarch64")
        # Alias expected by the aarch64 backend (`-bios QEMU_EFI.fd`).
        file(CREATE_LINK
            "${CURRENT_PACKAGES_DIR}/Resources/qemu/edk2-aarch64-code.fd"
            "${CURRENT_PACKAGES_DIR}/Resources/qemu/QEMU_EFI.fd"
            COPY_ON_ERROR)
    endif()
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
# QEMU ships application executables (e.g. qemu-img) in bin/, which is expected
# even for static-linkage triplets.
set(VCPKG_POLICY_ALLOW_EXES_IN_BIN enabled)
set(VCPKG_POLICY_DLLS_IN_STATIC_LIBRARY enabled)
vcpkg_install_copyright(FILE_LIST "${SOURCE_PATH}/COPYING")