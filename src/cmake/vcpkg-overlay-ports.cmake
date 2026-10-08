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

set(MULTIPASS_SOURCE_VCPKG_PORTS_DIR "${CMAKE_CURRENT_SOURCE_DIR}/3rd-party/vcpkg-ports")
set(MULTIPASS_UPSTREAM_VCPKG_PORTS_DIR "${MULTIPASS_VCPKG_LOCATION}/ports")
set(MULTIPASS_PATCHES_DIR_NAME "multipass-patches")

# A port is patched when its 3rd-party/vcpkg-ports dir has multipass-patches/portfile.cmake.patch
set(MULTIPASS_PORTFILE_PATCH "${MULTIPASS_PATCHES_DIR_NAME}/portfile.cmake.patch")

# Find the names of the upstream vcpkg ports that we patch
function(find_patched_vcpkg_ports OUT_PORTS)
    file(GLOB PORTS RELATIVE "${MULTIPASS_SOURCE_VCPKG_PORTS_DIR}"
        "${MULTIPASS_SOURCE_VCPKG_PORTS_DIR}/*/${MULTIPASS_PORTFILE_PATCH}")
    list(TRANSFORM PORTS REPLACE "/${MULTIPASS_PORTFILE_PATCH}$" "")
    set(${OUT_PORTS} "${PORTS}" PARENT_SCOPE)
endfunction()

# Check that the vcpkg PORT that we patch exists upstream and that we keep only our patches for it
function(validate_patched_vcpkg_port PORT)
    if(NOT IS_DIRECTORY "${MULTIPASS_UPSTREAM_VCPKG_PORTS_DIR}/${PORT}")
        message(FATAL_ERROR
            "Cannot patch vcpkg port ${PORT}: not found in ${MULTIPASS_UPSTREAM_VCPKG_PORTS_DIR}")
    endif()
endfunction()

# Copy the upstream vcpkg PORT, along with our patches for it, into DESTINATION
function(copy_vcpkg_port PORT DESTINATION)
    file(COPY "${MULTIPASS_UPSTREAM_VCPKG_PORTS_DIR}/${PORT}/" DESTINATION "${DESTINATION}")
    file(COPY "${MULTIPASS_SOURCE_VCPKG_PORTS_DIR}/${PORT}/${MULTIPASS_PATCHES_DIR_NAME}"
        DESTINATION "${DESTINATION}")
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
        apply_vcpkg_port_patch("${PORT_DIR}/${MULTIPASS_PORTFILE_PATCH}" "${PORT_DIR}")
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
