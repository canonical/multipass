# Reads the pinned MinGW closure into the requested output variables.
function(multipass_get_mingw_packages OUT_DIRECT_PACKAGES OUT_URLS OUT_SHA512S)
    if(NOT ARGC EQUAL 3)
        message(FATAL_ERROR "multipass_get_mingw_packages requires direct-packages, URLs, and SHA512 output variables")
    endif()

    set(_lockfile "${CMAKE_CURRENT_FUNCTION_LIST_DIR}/mingw64-toolchain.lock")
    set(_expected_lock_sha512
        "13e40c78ddbcbbec011257e8bd0496d52c3641d57ed3ee0f61a1a415876438f3e87e556643d8d39b6fca934001e9eec3efd64e9e2523a4c8aeff30c05b18eede")
    file(SHA512 "${_lockfile}" _lockfile_sha512)

    # To ensure that if we changed the lockfile, we have to change this file as well, prompting
    # vcpkg to build the port that included this file.
    if(NOT "${_lockfile_sha512}" STREQUAL "${_expected_lock_sha512}")
        message(FATAL_ERROR "mingw64-toolchain.lock changed; update its expected SHA512 in get-mingw-packages.cmake")
    endif()

    set(_direct_packages "")
    set(_urls "")
    set(_sha512s "")
    file(STRINGS "${_lockfile}" _lock_lines)
    foreach(_line IN LISTS _lock_lines)
        if(_line MATCHES "^#" OR _line STREQUAL "")
            continue()
        endif()
        if(NOT _line MATCHES "^([^ \t]+)[ \t]+([0-9a-fA-F]+)$")
            message(FATAL_ERROR "Malformed mingw64-toolchain.lock entry: ${_line}")
        endif()
        list(APPEND _direct_packages "${CMAKE_MATCH_1}" "${CMAKE_MATCH_2}")
        list(APPEND _urls "${CMAKE_MATCH_1}")
        list(APPEND _sha512s "${CMAKE_MATCH_2}")
    endforeach()

    set(${OUT_DIRECT_PACKAGES} "${_direct_packages}" PARENT_SCOPE)
    set(${OUT_URLS} "${_urls}" PARENT_SCOPE)
    set(${OUT_SHA512S} "${_sha512s}" PARENT_SCOPE)
endfunction()
