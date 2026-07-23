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
        multipass-patches/0005-disable-werror-for-dtc-subproject.patch
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
    "--disable-png"
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
    # Windows: build only qemu-img (the sole QEMU tool Multipass uses here; no
    # system emulator). The GCC/MinGW toolchain is acquired hermetically via
    # vcpkg_acquire_msys() from packages pinned by URL + SHA512 -- vcpkg's own
    # pinned msys shell layer plus the MinGW closure in mingw64-toolchain.lock
    # (regenerate with generate-mingw-lockfile.py).
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

    # Pinned MinGW-w64 closure (regenerate with generate-mingw-lockfile.py).
    # Each non-comment line is "<url> <sha512>". Keep an interleaved list for
    # vcpkg_acquire_msys(DIRECT_PACKAGES ...) plus parallel url/sha lists.
    set(MINGW_DIRECT_PACKAGES "")
    set(MINGW_URLS "")
    set(MINGW_SHA512S "")
    file(STRINGS "${CMAKE_CURRENT_LIST_DIR}/mingw64-toolchain.lock" _lock_lines)
    foreach(_line IN LISTS _lock_lines)
        if(_line MATCHES "^#" OR _line STREQUAL "")
            continue()
        endif()
        if(NOT _line MATCHES "^([^ \t]+)[ \t]+([0-9a-fA-F]+)$")
            message(FATAL_ERROR "Malformed mingw64-toolchain.lock entry: ${_line}")
        endif()
        list(APPEND MINGW_DIRECT_PACKAGES "${CMAKE_MATCH_1}" "${CMAKE_MATCH_2}")
        list(APPEND MINGW_URLS "${CMAKE_MATCH_1}")
        list(APPEND MINGW_SHA512S "${CMAKE_MATCH_2}")
    endforeach()

    # Assemble a private, content-addressed MSYS2 root: vcpkg's pinned msys shell
    # utilities (plus findutils/perl for configure) and our pinned MinGW closure.
    vcpkg_acquire_msys(MSYS_ROOT
        PACKAGES findutils perl
        DIRECT_PACKAGES ${MINGW_DIRECT_PACKAGES}
    )

    # Run the MSYS2 tools under an explicit MINGW64 environment (no login shell):
    # mingw64/bin and usr/bin on PATH, MSYSTEM selecting the MinGW64 personality.
    file(TO_NATIVE_PATH "${MSYS_ROOT}/mingw64/bin" _mingw_bin_native)
    file(TO_NATIVE_PATH "${MSYS_ROOT}/usr/bin" _usr_bin_native)
    set(ENV{PATH} "${_mingw_bin_native};${_usr_bin_native};$ENV{PATH}")
    set(ENV{MSYSTEM} "MINGW64")

    macro(msys2_exec COMMAND_STRING LOGNAME_SUFFIX)
        vcpkg_execute_required_process(
            COMMAND "${MSYS_ROOT}/usr/bin/bash.exe" --noprofile --norc -c "${COMMAND_STRING}"
            WORKING_DIRECTORY "${MSYS_ROOT}"
            LOGNAME ${LOGNAME_SUFFIX}-${TARGET_TRIPLET}
        )
    endmacro()

    to_msys_path("${SOURCE_PATH}" SOURCE_PATH_UNIX)
    set(BUILD_DIR "${CURRENT_BUILDTREES_DIR}/${TARGET_TRIPLET}-rel")
    file(MAKE_DIRECTORY "${BUILD_DIR}")
    to_msys_path("${BUILD_DIR}" BUILD_DIR_UNIX)
    to_msys_path("${CURRENT_PACKAGES_DIR}" INSTALL_PREFIX_UNIX)

    # vcpkg_acquire_msys() extracts the full MinGW packages but then deletes
    # every mingw64/lib/pkgconfig on *every* call (the removal sits outside its
    # "assemble the root once" guard). qemu-img's meson build needs the glib/zstd
    # .pc files, so we keep a private backup of them (harvested once, gated on the
    # backup dir) and copy it back into the canonical mingw64/lib/pkgconfig on
    # every run. The canonical location matters: pkgconf's --define-prefix (on by
    # default) only rewrites each .pc's absolute `prefix=/mingw64` into the real
    # Windows path when the file is found under a `.../lib/pkgconfig` layout, and
    # native mingw gcc can't resolve the bare `/mingw64` POSIX paths otherwise.
    set(PKGCONFIG_BACKUP "${MSYS_ROOT}/.multipass-qemu-pkgconfig")
    if(NOT EXISTS "${PKGCONFIG_BACKUP}")
        # Re-extract the pinned packages into a scratch dir and copy out their
        # .pc files (the headers/libs acquire already installed are untouched).
        set(_harvest "${CURRENT_BUILDTREES_DIR}/pkgconfig-harvest")
        file(REMOVE_RECURSE "${_harvest}")
        list(LENGTH MINGW_URLS _mingw_count)
        math(EXPR _mingw_last "${_mingw_count} - 1")
        foreach(_i RANGE ${_mingw_last})
            list(GET MINGW_URLS ${_i} _url)
            list(GET MINGW_SHA512S ${_i} _sha)
            get_filename_component(_fn "${_url}" NAME)
            vcpkg_download_distfile(_archive URLS "${_url}" FILENAME "${_fn}" SHA512 "${_sha}" QUIET)
            file(ARCHIVE_EXTRACT INPUT "${_archive}" DESTINATION "${_harvest}")
        endforeach()
        file(GLOB _pc_files "${_harvest}/mingw64/lib/pkgconfig/*.pc")
        file(COPY ${_pc_files} DESTINATION "${PKGCONFIG_BACKUP}")
        file(REMOVE_RECURSE "${_harvest}")
    endif()
    file(GLOB _pc_backup "${PKGCONFIG_BACKUP}/*.pc")
    file(COPY ${_pc_backup} DESTINATION "${MSYS_ROOT}/mingw64/lib/pkgconfig")

    # meson build deps: distlib + QEMU's vendored meson wheel. site-packages
    # survives acquire's deletions, so this is a genuine one-time step.
    set(PYTHON_STAMP "${MSYS_ROOT}/.multipass-qemu-python-ready")
    if(NOT EXISTS "${PYTHON_STAMP}")
        msys2_exec("rm -f /mingw64/lib/python3.*/EXTERNALLY-MANAGED && python -m ensurepip && python -m pip install distlib '${SOURCE_PATH_UNIX}'/python/wheels/*.whl" msys2-python-deps)
        file(TOUCH "${PYTHON_STAMP}")
    endif()

    # Build only qemu-img: --enable-tools exposes the target, --disable-system
    # skips the system emulator, --disable-docs avoids the sphinx dependency, and
    # pixman/zstd are pinned off/on so features don't depend on autodetection.
    # PKG_CONFIG_PATH/LIBDIR are cleared so pkgconf falls back to its default
    # search (the canonical mingw64/lib/pkgconfig we restore above), which is what
    # triggers --define-prefix to emit real Windows include/lib paths. --prefix is
    # required (meson validates it) though we never install; the binary is copied
    # straight from the build tree.
    # QEMU on Windows uses the hermetic MinGW/GCC toolchain from vcpkg_acquire_msys(),
    # not MSVC -- unset CC/CXX so the MinGW bash shell picks up its own gcc.
    unset(ENV{CC})
    unset(ENV{CXX})
    msys2_exec("unset PKG_CONFIG_PATH PKG_CONFIG_LIBDIR && cd '${BUILD_DIR_UNIX}' && '${SOURCE_PATH_UNIX}/configure' --static --enable-tools --disable-system --disable-docs --disable-pixman --enable-zstd --prefix='${INSTALL_PREFIX_UNIX}' --extra-cflags='-UNDEBUG' --extra-ldflags='-liconv'" configure)
    msys2_exec("cd '${BUILD_DIR_UNIX}' && ninja -j${VCPKG_CONCURRENCY} qemu-img.exe" build)

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
    file(COPY_FILE "${CMAKE_CURRENT_LIST_DIR}/configure-wrapper" "${SOURCE_PATH}/configure")
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

    # Ship the 512 KB iPXE virtio-net option ROM instead of QEMU's smaller
    # in-tree copy, so instances suspended by older Multipass releases can still
    # be resumed. See vendor/README.md for details.
    file(COPY "${CMAKE_CURRENT_LIST_DIR}/vendor/efi-virtio.rom" DESTINATION "${CURRENT_PACKAGES_DIR}/Resources/qemu")

    if(QEMU_ARCH STREQUAL "x86_64")
        # Ship the legacy 2 MB combined OVMF image so instances suspended by
        # older Multipass releases (which booted with `-bios OVMF.fd`) can still
        # be resumed. See vendor/README.md for details.
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