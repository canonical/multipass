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
#

function(add_ssh_target TARGET_NAME)
  add_library(${TARGET_NAME} STATIC
    openssh_key_provider.cpp
    ssh_client_key_provider.cpp
    ssh_process.cpp
    ssh_session.cpp)

  target_link_libraries(${TARGET_NAME}
    fmt
    libssh
    utils
    Qt6::Core)
endfunction()

add_ssh_target(ssh_common)
if(MULTIPASS_ENABLE_TESTS)
  add_ssh_target(ssh_test)
endif()

function(add_sftp_client_target TARGET_NAME)
  add_library(${TARGET_NAME} STATIC
    sftp_client.cpp
    sftp_dir_iterator.cpp
    sftp_utils.cpp
    ssh_session.cpp)

  target_link_libraries(${TARGET_NAME}
    fmt
    libssh
    utils
    Qt6::Core)
endfunction()

add_sftp_client_target(sftp_client)
if(MULTIPASS_ENABLE_TESTS)
  add_sftp_client_target(sftp_test)
endif()

function(add_ssh_client_target TARGET_NAME)
  add_library(${TARGET_NAME} STATIC
    ssh_client.cpp
    ssh_session.cpp)

  target_link_libraries(${TARGET_NAME}
    console
    fmt
    libssh
    utils
    Qt6::Core)
endfunction()

add_ssh_client_target(ssh_client)
if(MULTIPASS_ENABLE_TESTS)
  add_ssh_client_target(ssh_client_test)
endif()
