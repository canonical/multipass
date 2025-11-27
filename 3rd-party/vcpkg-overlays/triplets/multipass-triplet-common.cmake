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

list(APPEND VCPKG_CMAKE_CONFIGURE_OPTIONS "-DCMAKE_PROJECT_INCLUDE:STRING=${CMAKE_CURRENT_LIST_DIR}/cmake/multipass-vcpkg-target-filter.cmake")
set(MP_VCPKG_TRIPLETS_DIR "${CMAKE_CURRENT_LIST_DIR}")
cmake_path(GET CMAKE_CURRENT_LIST_DIR PARENT_PATH MP_VCPKG_OVERLAYS_DIR)
cmake_path(APPEND MP_VCPKG_OVERLAYS_DIR "target-filters" OUTPUT_VARIABLE MP_VCPKG_TARGET_FILTERS_DIR)

# Get all packages with hooks
file(GLOB STUFF_IN_HOOKS_DIR RELATIVE ${MP_VCPKG_TARGET_FILTERS_DIR} ${MP_VCPKG_TARGET_FILTERS_DIR}/*/)
set(MP_VCPKG_PACKAGES_WITH_HOOKS "")
foreach(entry ${STUFF_IN_HOOKS_DIR})
    if(IS_DIRECTORY ${MP_VCPKG_TARGET_FILTERS_DIR}/${entry})
        if(EXISTS "${MP_VCPKG_TARGET_FILTERS_DIR}/${entry}/include.cmake")
            include("${MP_VCPKG_TARGET_FILTERS_DIR}/${entry}/include.cmake")
            string(JOIN ";" MP_VCPKG_DISABLE_CMAKE_TARGETS_FLATTENED "${MP_VCPKG_DISABLE_CMAKE_TARGETS}")
            # Propagate to the downstream package
            list(APPEND VCPKG_CMAKE_CONFIGURE_OPTIONS "-DMP_VCPKG_DISABLE_${entry}_CMAKE_TARGETS:STRING=\"${MP_VCPKG_DISABLE_CMAKE_TARGETS_FLATTENED}\"")
            # Also set it for the portfile.cmake context
            set(MP_VCPKG_DISABLE_${entry}_CMAKE_TARGETS ${MP_VCPKG_DISABLE_CMAKE_TARGETS})
        endif()
    endif()
endforeach()

function(vcpkg_copy_tools)
    set(ARGN_MODIFIED ${ARGN})
    cmake_path(GET CMAKE_CURRENT_LIST_DIR PARENT_PATH CALLING_PACKAGE_NAME)
    cmake_path(GET CALLING_PACKAGE_NAME FILENAME CALLING_PACKAGE_NAME)
    set(varname "MP_VCPKG_DISABLE_${CALLING_PACKAGE_NAME}_CMAKE_TARGETS")
    if(DEFINED ${varname})
        # Remove lines verbatim
        foreach(line ${${varname}})
            list(REMOVE_ITEM ARGN_MODIFIED ${line})
        endforeach()
    endif()
    message(DEBUG "called: vcpkg_copy_tools(${CALLING_PACKAGE_NAME}): BEFORE ${ARGN}, AFTER ${ARGN_MODIFIED}")
    _vcpkg_copy_tools(${ARGN_MODIFIED})
endfunction()


