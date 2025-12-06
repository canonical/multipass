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

function(add_target TARGET_NAME)
  add_library(${TARGET_NAME} STATIC
    file_ops.cpp
    memory_size.cpp
    permission_utils.cpp
    json_utils.cpp
    snap_utils.cpp
    standard_paths.cpp
    timer.cpp
    utils.cpp
    vm_image_vault_utils.cpp
    vm_mount.cpp
    yaml_node_utils.cpp
    semver_compare.cpp)

  target_link_libraries(${TARGET_NAME}
    PUBLIC
    cert
    fmt::fmt-header-only
    logger
    ssh_common
    ssh
    yaml-cpp::yaml-cpp
    xz_image_decoder
    Qt6::Core
    PRIVATE semver::semver
    )

  target_include_directories(${TARGET_NAME} PRIVATE
    ${OPENSSL_INCLUDE_DIR})

  target_compile_definitions(${TARGET_NAME} PUBLIC O_BINARY=0)
endfunction()

add_target(utils)
if(MULTIPASS_ENABLE_TESTS)
  add_target(utils_test)
endif()
