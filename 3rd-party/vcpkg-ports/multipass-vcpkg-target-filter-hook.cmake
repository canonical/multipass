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

# So... The code below needs some explaining to do.
#
# Sometimes, we'd like to modify some of our vcpkg dependencies. The official
# way to do that is through providing a portfile override for that particular
# dependency. Then, the dependency may be modified through applying patch files.
# This all works fine for small patches, but for one of our use-cases, which is
# trimming down gRPC by eliminating unnecessary targets, the patch is huge and
# it's painful to adapt that patch across newer releases of gRPC, which effectively
# blocks the version upgrades.
#
# Instead of doing that, this approach provides a CMake module which is meant to be
# included with the first `project()` call of the target, and overrides a bunch of
# target-defining functions, like add_executable, add_library and install. When a
# defined function is redefined by user-code, CMake automatically renames the original
# by prefixing it with an underscore, e.g.,:
#
# function(add_executable)
# # .... the real add_executable is now _add_executable()
# endfunction()
#
# The code is utilizing this approach to eliminate a bunch of unneeded targets. Sharp-eyed
# readers might have noticed that we're overriding the _add_executable, _add_library, and
# _install instead of the regular functions. The reason is that "vcpkg" itself is using the
# same trick to do some post-processing hence the original is already been overridden before
# us. Therefore, we're overriding their override instead (... kinda like inception).
#
# One downside with this approach is it's relying on undocumented behavior as described here:
# https://crascit.com/2018/09/14/do-not-redefine-cmake-commands/
#
# ... but, hey. vcpkg itself is using this, so why shouldn't we?
# 
# https://github.com/microsoft/vcpkg/blob/c941d5e450738629213a60ab8911fd26efca7c6e/scripts/buildsystems/vcpkg.cmake#L647
# https://cmake.org/cmake/help/latest/variable/CMAKE_PROJECT_PROJECT-NAME_INCLUDE_BEFORE.html

separate_arguments(MP_VCPKG_TARGET_FILTER_SKIP)

function(maybe_exclude_target TARGET_NAME)
    if(TARGET_NAME IN_LIST MP_VCPKG_TARGET_FILTER_SKIP)
        message(STATUS "${TARGET_NAME} is excluded from ALL")
        set_property(TARGET "${TARGET_NAME}" PROPERTY EXCLUDE_FROM_ALL TRUE)
    endif()
endfunction()

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
            if(target IN_LIST MP_VCPKG_TARGET_FILTER_SKIP)
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

function(_add_executable)
    list(GET ARGN 0 target_name)
    __add_executable(${ARGN})
    maybe_exclude_target(${target_name})
endfunction()

function(_add_library)
    list(GET ARGN 0 target_name)
    __add_library(${ARGN})
    maybe_exclude_target(${target_name})
endfunction()