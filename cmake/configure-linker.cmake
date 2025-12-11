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
            set(MOLD_LINKER_NAMES /usr/libexec/mold/ld /usr/local/libexec/mold/ld mold sold)
            find_program(MOLD_LINKER NAMES ${MOLD_LINKER_NAMES})
        endif()

        find_program(LLD_LINKER NAMES ${LLD_LINKER_NAMES})

        if(MOLD_LINKER)
            message(STATUS "configure_linker: `mold` (${MOLD_LINKER}) found in environment, will use it as default.")
            set(CMAKE_LINKER_TYPE MOLD PARENT_SCOPE)
            if(MOLD_LINKER MATCHES "libexec")
                string(REPLACE "/ld" "" MOLD_LINKER_PATH ${MOLD_LINKER})
                #
                # The reason why we're setting this variables is that GCC < 12.1 does not
                # support the "-fuse-ld=mold" parameter, which causes CMake configure to fail
                # when configured with mold + < GCC 12.1 combo. There's a detailed discussion
                # about this in CMake issue board: https://gitlab.kitware.com/cmake/cmake/-/issues/25748#note_1494202
                # The gist of itis that CMake maintainers decided to not to cover this edge case
                # so we have to intervene.
                #
                # CMake will override this with "-fuse-ld=mold" for clang and GCC >= 12.1
                #
                set(CMAKE_C_USING_LINKER_MOLD "-B${MOLD_LINKER_PATH}" PARENT_SCOPE)
                set(CMAKE_CXX_USING_LINKER_MOLD "-B${MOLD_LINKER_PATH}" PARENT_SCOPE)
            endif()
        elseif(LLD_LINKER)
            message(STATUS "configure_linker: `lld` (${LLD_LINKER}) found in environment, will use it as default.")
            set(CMAKE_LINKER_TYPE LLD PARENT_SCOPE)
        else()
            message(STATUS "configure_linker: could use neither `mold` nor `lld`; will use the toolchain default.")
        endif()
    else()
        message(STATUS "configure_linker: will use the user specified linker: `${CMAKE_LINKER_TYPE}`")
    endif()
endfunction()
