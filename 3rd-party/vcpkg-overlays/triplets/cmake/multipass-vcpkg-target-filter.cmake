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

# This file is intended to be "injected" into the target dependency's CMake configure step.

include_guard(GLOBAL)

if(DEFINED MP_VCPKG_DISABLE_${PROJECT_NAME}_CMAKE_TARGETS)
    separate_arguments(MP_VCPKG_DISABLE_${PROJECT_NAME}_CMAKE_TARGETS)
endif()

function(do_target_postprocessing)
    get_property(DIR_TARGETS DIRECTORY PROPERTY BUILDSYSTEM_TARGETS)
    foreach(TARGET_NAME IN LISTS DIR_TARGETS)
        if(TARGET_NAME IN_LIST MP_VCPKG_DISABLE_${PROJECT_NAME}_CMAKE_TARGETS)
            message(STATUS "${TARGET_NAME} is excluded from ALL")
            set_property(TARGET "${TARGET_NAME}" PROPERTY EXCLUDE_FROM_ALL TRUE)
        endif()
    endforeach()

endfunction()

cmake_language(DEFER CALL do_target_postprocessing)

# vcpkg conditionally overrides the install, so check that.
if(COMMAND _install)
    set(INSTALL_TO_HOOK _install)
else()
    set(INSTALL_TO_HOOK install)
endif()

function(${INSTALL_TO_HOOK})
    # Parse the TARGETS from install command
    cmake_parse_arguments(ARG "" "" "TARGETS" ${ARGN})

    if(ARG_TARGETS)
        foreach(target ${ARG_TARGETS})
            if(target IN_LIST MP_VCPKG_DISABLE_${PROJECT_NAME}_CMAKE_TARGETS)
                # If we get here, skip this install command
                message(STATUS "Skipping install for: ${ARG_TARGETS}")
                return()
            endif()
        endforeach()
        cmake_language(CALL "_${INSTALL_TO_HOOK}" ${ARGN})
    else()
        cmake_language(CALL "_${INSTALL_TO_HOOK}" ${ARGN})
    endif()
endfunction()