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

# Each dir in 3rd-party/vcpkg-port-patches holds our patches for the upstream port of the same name
set(MULTIPASS_VCPKG_PORT_PATCHES_DIR "${CMAKE_CURRENT_SOURCE_DIR}/3rd-party/vcpkg-port-patches")
set(MULTIPASS_UPSTREAM_VCPKG_PORTS_DIR "${MULTIPASS_VCPKG_LOCATION}/ports")
set(MULTIPASS_PATCHES_DIR_NAME "multipass-patches") # where our patches go in generated ports
set(MULTIPASS_PORTFILE_PATCH_NAME "portfile.cmake.patch")

# Find the names of the upstream vcpkg ports that we patch
function(find_patched_vcpkg_ports OUT_PORTS)
    file(GLOB ENTRIES RELATIVE "${MULTIPASS_VCPKG_PORT_PATCHES_DIR}"
        "${MULTIPASS_VCPKG_PORT_PATCHES_DIR}/*")
    set(PORTS)
    foreach(ENTRY IN LISTS ENTRIES)
        if(IS_DIRECTORY "${MULTIPASS_VCPKG_PORT_PATCHES_DIR}/${ENTRY}")
            list(APPEND PORTS "${ENTRY}")
        endif()
    endforeach()
    set(${OUT_PORTS} "${PORTS}" PARENT_SCOPE)
endfunction()

# Check that the vcpkg PORT that we patch exists upstream and that we have a portfile patch for it
function(validate_patched_vcpkg_port PORT)
    if(NOT IS_DIRECTORY "${MULTIPASS_UPSTREAM_VCPKG_PORTS_DIR}/${PORT}")
        message(FATAL_ERROR
            "Cannot patch vcpkg port ${PORT}: not found in ${MULTIPASS_UPSTREAM_VCPKG_PORTS_DIR}")
    endif()

    set(PORT_PATCHES_DIR "${MULTIPASS_VCPKG_PORT_PATCHES_DIR}/${PORT}")
    if(NOT EXISTS "${PORT_PATCHES_DIR}/${MULTIPASS_PORTFILE_PATCH_NAME}")
        message(FATAL_ERROR "Cannot patch vcpkg port ${PORT}: "
            "${MULTIPASS_PORTFILE_PATCH_NAME} not found in ${PORT_PATCHES_DIR}")
    endif()
endfunction()

# Copy the upstream vcpkg PORT, along with our patches for it, into DESTINATION. Leave out our
# READMEs, so that documentation changes do not affect the port's ABI (forcing rebuilds).
function(copy_vcpkg_port PORT DESTINATION)
    file(COPY "${MULTIPASS_UPSTREAM_VCPKG_PORTS_DIR}/${PORT}/" DESTINATION "${DESTINATION}")
    file(COPY "${MULTIPASS_VCPKG_PORT_PATCHES_DIR}/${PORT}/"
        DESTINATION "${DESTINATION}/${MULTIPASS_PATCHES_DIR_NAME}"
        PATTERN "README.md" EXCLUDE)
endfunction()

# Apply PATCH to the files it names, relative to PORT_DIR, which must not be a git repo root
function(apply_vcpkg_port_patch PATCH PORT_DIR)
    if(EXISTS "${PORT_DIR}/.git")
        message(FATAL_ERROR "Cannot apply ${PATCH} in ${PORT_DIR}: it is a git repo")
    endif()

    find_package(Git REQUIRED)

    # Mirror vcpkg's own patching: works outside a repo, no fuzz. We don't call vcpkg's function
    # (z_vcpkg_apply_patches) directly, because it is private and needs a portfile context.
    execute_process(
        COMMAND "${CMAKE_COMMAND}" -E env GIT_CONFIG_NOSYSTEM=1
            "${GIT_EXECUTABLE}" -c core.longpaths=true -c core.autocrlf=false
            -c core.filemode=true --work-tree=. --git-dir=.git
            apply "${PATCH}" --ignore-whitespace --whitespace=nowarn
        WORKING_DIRECTORY "${PORT_DIR}"
        RESULT_VARIABLE APPLY_RESULT
        ERROR_VARIABLE APPLY_ERROR
    )
    if(NOT APPLY_RESULT EQUAL 0)
        message(FATAL_ERROR "Could not apply ${PATCH} in ${PORT_DIR}:\n${APPLY_ERROR}")
    endif()
endfunction()

# Generate, in PORTS_DIR, each upstream vcpkg port that we patch, with our patches applied
function(generate_patched_vcpkg_ports PORTS_DIR)
    file(REMOVE_RECURSE "${PORTS_DIR}")
    file(MAKE_DIRECTORY "${PORTS_DIR}")

    find_patched_vcpkg_ports(PORTS)
    foreach(PORT IN LISTS PORTS)
        validate_patched_vcpkg_port("${PORT}")
        set(PORT_DIR "${PORTS_DIR}/${PORT}")
        copy_vcpkg_port("${PORT}" "${PORT_DIR}")
        set(PATCHES_DIR "${PORT_DIR}/${MULTIPASS_PATCHES_DIR_NAME}")
        apply_vcpkg_port_patch("${PATCHES_DIR}/${MULTIPASS_PORTFILE_PATCH_NAME}" "${PORT_DIR}")
        message(STATUS "Generated patched vcpkg port: ${PORT}")
    endforeach()
endfunction()

# Register the ports in PORTS_DIR as vcpkg overlay ports
function(register_vcpkg_overlay_ports PORTS_DIR)
    # The vcpkg toolchain caches this, so avoid accumulating duplicates across reconfigurations
    list(APPEND VCPKG_OVERLAY_PORTS "${PORTS_DIR}")
    list(REMOVE_DUPLICATES VCPKG_OVERLAY_PORTS)
    set(VCPKG_OVERLAY_PORTS "${VCPKG_OVERLAY_PORTS}" PARENT_SCOPE)
endfunction()
