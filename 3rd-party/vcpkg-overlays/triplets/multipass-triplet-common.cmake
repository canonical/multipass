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

include_guard(GLOBAL)

list(APPEND VCPKG_CMAKE_CONFIGURE_OPTIONS "-DCMAKE_PROJECT_INCLUDE:STRING=${CMAKE_CURRENT_LIST_DIR}/cmake/multipass-vcpkg-target-filter-hook.cmake")
set(MP_VCPKG_TRIPLETS_DIR "${CMAKE_CURRENT_LIST_DIR}")
cmake_path(GET CMAKE_CURRENT_LIST_DIR PARENT_PATH MP_VCPKG_OVERLAYS_DIR)
cmake_path(APPEND MP_VCPKG_OVERLAYS_DIR "hooks" OUTPUT_VARIABLE MP_VCPKG_HOOKS_DIR)

# Get all packages with hooks
file(GLOB STUFF_IN_HOOKS_DIR RELATIVE ${MP_VCPKG_HOOKS_DIR} ${MP_VCPKG_HOOKS_DIR}/*/)
set(MP_VCPKG_PACKAGES_WITH_HOOKS "")
foreach(entry ${STUFF_IN_HOOKS_DIR})
    if(IS_DIRECTORY ${MP_VCPKG_HOOKS_DIR}/${entry})
        list(APPEND MP_VCPKG_PACKAGES_WITH_HOOKS ${entry})
    endif()
endforeach()

message(DEBUG "Hooked vcpkg packages: ${MP_VCPKG_PACKAGES_WITH_HOOKS}")

macro(include)
    file(TO_CMAKE_PATH ${ARGV0} NORMALIZED_PATH)
    get_filename_component(MODULE_NAME "${NORMALIZED_PATH}" NAME)

    # Check if a portfile is being loaded
    if (MODULE_NAME STREQUAL "portfile.cmake")
        # Go two levels up
        get_filename_component(PARENT "${NORMALIZED_PATH}" DIRECTORY)
        get_filename_component(PARENT "${PARENT}" DIRECTORY)
        # Extract the last component → package name
        get_filename_component(PACKAGE_NAME "${PARENT}" NAME)
        message(DEBUG "A portfile.cmake is being loaded: ${PACKAGE_NAME}")
        list(FIND MP_VCPKG_PACKAGES_WITH_HOOKS ${PACKAGE_NAME} FOUND_PACKAGE_INDEX)

        if(NOT FOUND_PACKAGE_INDEX EQUAL -1)
            _include("${MP_VCPKG_TRIPLETS_DIR}/cmake/multipass-vcpkg-cmake.cmake")

            if(EXISTS "${MP_VCPKG_HOOKS_DIR}/${PACKAGE_NAME}/pre_portfile.cmake")
                _include("${MP_VCPKG_HOOKS_DIR}/${PACKAGE_NAME}/pre_portfile.cmake")
            endif()

            _include(${ARGN})

            if(EXISTS "${MP_VCPKG_HOOKS_DIR}/${PACKAGE_NAME}/post_portfile.cmake")
                _include("${MP_VCPKG_HOOKS_DIR}/${PACKAGE_NAME}/post_portfile.cmake")
            endif()

        else()
            _include(${ARGN})
        endif()
    else()
        _include(${ARGN})
    endif()
endmacro()
