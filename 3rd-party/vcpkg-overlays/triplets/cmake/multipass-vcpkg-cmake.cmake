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

function(vcpkg_cmake_configure)
    set(ARGN_MODIFIED ${ARGN})
    if(DEFINED MP_VCPKG_DISABLE_CMAKE_TARGETS)
        string(JOIN ";" MP_VCPKG_DISABLE_CMAKE_TARGETS_FLATTENED "${MP_VCPKG_DISABLE_CMAKE_TARGETS}")
        list(APPEND VCPKG_CMAKE_CONFIGURE_OPTIONS "-DMP_VCPKG_DISABLE_CMAKE_TARGETS:STRING=\"${MP_VCPKG_DISABLE_CMAKE_TARGETS_FLATTENED}\"")
    endif()
    _vcpkg_cmake_configure(${ARGN_MODIFIED})
endfunction()

function(vcpkg_copy_tools)
    set(ARGN_MODIFIED ${ARGN})
    if(DEFINED MP_VCPKG_DISABLE_CMAKE_TARGETS)
        # Remove lines verbatim
        foreach(line ${MP_VCPKG_DISABLE_CMAKE_TARGETS})
            list(REMOVE_ITEM ARGN_MODIFIED ${line})
        endforeach()
        message(DEBUG "called: vcpkg_copy_tools: BEFORE ${ARGN}, AFTER ${ARGN_MODIFIED}")
    endif()
    _vcpkg_copy_tools(${ARGN_MODIFIED})
endfunction()
