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

# Now, we have to patch a bunch of things in the original portfile.
# Read the contents first
file(READ "${CMAKE_CURRENT_LIST_DIR}/portfile-original.cmake" _original_portfile_contents)

# These plugins aren't going to be available since we're disabling their targets.
# We have to comment out them in the portfile, too.
set(PLUGINS_TO_SKIP
    grpc_php_plugin
    grpc_python_plugin
    grpc_node_plugin
    grpc_objective_c_plugin
    grpc_csharp_plugin
    grpc_ruby_plugin
)

set(TARGETS_TO_SKIP
  grpc_unsecure
  grpc++_unsecure
  grpc_authorization_provider
  grpc_objective_c_plugin
  grpc++_alts
  grpc++_reflection
  ${PLUGINS_TO_SKIP}
)

# Iterate and comment out plugins
foreach(plugin IN LISTS PLUGINS_TO_SKIP)
    string(REGEX REPLACE
        "([^#\n]*${plugin})"
        [[#\1]]
        _original_portfile_contents
        "${_original_portfile_contents}")
endforeach()

# Utilize CMAKE_PROJECT_<PROJECT_NAME>_INCLUDE to automatically load our hook.
# Also pass the names of the targets to filter.
set(ADDITIONAL_OPTIONS_TEMPLATE [[-DCMAKE_PROJECT_grpc_INCLUDE=${CMAKE_CURRENT_LIST_DIR}/multipass-vcpkg-target-filter-hook.cmake
    -DMP_VCPKG_TARGET_FILTER_SKIP="@TARGETS_TO_SKIP@"]])

list(JOIN TARGETS_TO_SKIP ";" TARGETS_TO_SKIP_STR)
string(REPLACE "@TARGETS_TO_SKIP@" "${TARGETS_TO_SKIP_STR}" ADDITIONAL_OPTIONS "${ADDITIONAL_OPTIONS_TEMPLATE}")

# Squeeze a couple of additional options to load the hook.
string(REPLACE
    [[OPTIONS ${FEATURE_OPTIONS}]]
    "OPTIONS \$\{FEATURE_OPTIONS\}
        ${ADDITIONAL_OPTIONS}"
    _original_portfile_contents
    "${_original_portfile_contents}"
)

# Write the altered portfile back.
file(WRITE "${CMAKE_CURRENT_LIST_DIR}/portfile-original.cmake" "${_original_portfile_contents}")

# Finally, call the portfile to set the things in motion.
include("${CMAKE_CURRENT_LIST_DIR}/portfile-original.cmake")