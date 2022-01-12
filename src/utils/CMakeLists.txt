# Copyright © 2017-2021 Canonical Ltd.
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
    json_writer.cpp
    settings.cpp
    snap_utils.cpp
    standard_paths.cpp
    timer.cpp
    utils.cpp
    vm_image_vault_utils.cpp)

  target_link_libraries(${TARGET_NAME}
    cert
    fmt
    logger
    ssh_common
    ssh
    yaml
    xz_image_decoder
    Qt5::Core
    Qt5::Gui)

  target_include_directories(${TARGET_NAME} PRIVATE
    ${OPENSSL_INCLUDE_DIR})
endfunction()

add_target(utils)
if(MULTIPASS_ENABLE_TESTS)
  add_target(utils_test)
endif()

add_library(poco_utils
  poco_zip_utils.cpp)

target_link_libraries(poco_utils
  Poco::Zip)
