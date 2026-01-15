macro(multipass_inject_vcpkg_patches)
    # Needs to be captured now -- vcpkg_extract_source_archive call will happen
    # at the portfile context.
    set(_MP_PATCH_DIR "${CMAKE_CURRENT_LIST_DIR}/../vcpkg-patches/${PORT}")

    # Defined as verbatim string and EVAL'ed to avoid having to escape the
    # variables in the nested macro.
    string(CONFIGURE [=[
        macro(vcpkg_extract_source_archive)
            set(_ARGN_MODIFIED ${ARGN})
            set(_PATCH_DIR "@_MP_PATCH_DIR@")

            file(GLOB _EXTRA_PATCHES "${_PATCH_DIR}/*.patch")
            if(_EXTRA_PATCHES)
                message(STATUS "Injecting Multipass patchset for vcpkg port `${PORT}`:")
                foreach(_patch IN LISTS _EXTRA_PATCHES)
                    get_filename_component(_patch_name "${_patch}" NAME)
                    message(STATUS "\t${_patch_name}")
                endforeach()

                list(FIND _ARGN_MODIFIED "PATCHES" _idx)
                if(_idx EQUAL -1)
                    list(APPEND _ARGN_MODIFIED PATCHES ${_EXTRA_PATCHES})
                else()
                    math(EXPR _idx "${_idx} + 1")
                    list(INSERT _ARGN_MODIFIED ${_idx} ${_EXTRA_PATCHES})
                endif()
            endif()

            _vcpkg_extract_source_archive(${_ARGN_MODIFIED})
        endmacro()
    ]=] _CODE @ONLY)

    cmake_language(EVAL CODE "${_CODE}")
endmacro()


# Per-port customization
if(PORT STREQUAL "libssh")
    multipass_inject_vcpkg_patches()
endif()