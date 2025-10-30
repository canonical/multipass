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

# These plugins aren't going to be available since we're disabling their targets.
# We have to comment out them in the portfile, too.
set(PLUGINS_TO_SKIP "grpc_php_plugin grpc_python_plugin grpc_node_plugin grpc_objective_c_plugin grpc_csharp_plugin grpc_ruby_plugin")
set(TARGETS_TO_SKIP "grpc_unsecure grpc++_unsecure grpc_authorization_provider grpc_objective_c_plugin grpc++_alts grpc++_reflection ${PLUGINS_TO_SKIP}")

include(${CMAKE_CURRENT_LIST_DIR}/multipass-override-vcpkg-function.cmake)

# Intercept the original vcpkg CMake functions to make a few adjustments.
override_vcpkg_function(
    FUNCTION_NAME vcpkg_cmake_configure
    ADD_OPTIONS
        "-DCMAKE_PROJECT_grpc_INCLUDE=${CMAKE_CURRENT_LIST_DIR}/multipass-vcpkg-target-filter-hook.cmake"
        -DMP_VCPKG_TARGET_FILTER_SKIP=${TARGETS_TO_SKIP}
)

override_vcpkg_function(
    FUNCTION_NAME vcpkg_copy_tools
    REMOVE_LINES ${PLUGINS_TO_SKIP}
)

# Finally, call the portfile to set the things in motion.
include("${CMAKE_CURRENT_LIST_DIR}/portfile-original.cmake")