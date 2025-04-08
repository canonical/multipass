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


function(configure_linker)
    if(${CMAKE_VERSION} VERSION_LESS "3.29")
        message(STATUS "configure_linker is only supported in CMake 3.29 and above.")
        return()
    endif()

    if(NOT DEFINED CMAKE_LINKER_TYPE)
        if (WIN32)
            set (LLD_LINKER_NAMES lld-link)
        else()
            set(LLD_LINKER_NAMES ld.lld ld64.lld)
            set(MOLD_LINKER_NAMES mold sold)
            find_program(MOLD_LINKER NAMES ${MOLD_LINKER_NAMES})
        endif()

        find_program(LLD_LINKER NAMES ${LLD_LINKER_NAMES})

        if(MOLD_LINKER)
            message(STATUS "configure_linker: `mold` found in environment, will use it as default.")
            set(CMAKE_LINKER_TYPE MOLD PARENT_SCOPE)
        elseif(LLD_LINKER)
            message(STATUS "configure_linker: `lld` found in environment, will use it as default.")
            set(CMAKE_LINKER_TYPE LDD PARENT_SCOPE)
        else()
            message(STATUS "configure_linker: neither `mold` or `lld` is present in the environment, will use the toolchain default.")
        endif()
    else()
        message(STATUS "configure_linker: will use the user specified linker: `${CMAKE_LINKER_TYPE}`")
    endif()
endfunction()
