include("${CMAKE_CURRENT_LIST_DIR}/get-mingw-packages.cmake")

# Acquire a shared MSYS2 root for all ports to avoid repeatedly setting up the
# same environment.
function(multipass_acquire_mingw_msys OUT_ROOT)
    multipass_get_mingw_packages(MINGW_DIRECT_PACKAGES MINGW_URLS MINGW_SHA512S)
    vcpkg_acquire_msys(MSYS_ROOT
        PACKAGES findutils perl
        DIRECT_PACKAGES ${MINGW_DIRECT_PACKAGES}
    )

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

    set(${OUT_ROOT} "${MSYS_ROOT}" PARENT_SCOPE)
endfunction()
