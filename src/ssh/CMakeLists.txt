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

# TODO: drop premock in favor of MP_LIBSSH, then remove all duplicate _test targets
function(add_libssh_target TARGET_NAME)
  add_library(${TARGET_NAME} STATIC
    libssh_wrapper.cpp)

  target_compile_definitions(${TARGET_NAME} PRIVATE WITH_SERVER)

  target_link_libraries(${TARGET_NAME}
    fmt::fmt-header-only
    ssh
    utils
    Qt6::Core)
endfunction()

add_libssh_target(libssh_wrapper)
if(MULTIPASS_ENABLE_TESTS)
  add_libssh_target(libssh_wrapper_test)
endif()

function(add_ssh_target TARGET_NAME LIBSSH_TARGET)
  add_library(${TARGET_NAME} STATIC
    ssh_factory.cpp
    libssh_scope_guard.cpp
    openssh_key_provider.cpp
    plain_ssh_process.cpp
    plain_ssh_session.cpp)

  target_link_libraries(${TARGET_NAME}
    ${LIBSSH_TARGET}
    fmt::fmt-header-only
    ssh
    utils
    Qt6::Core)
endfunction()

add_ssh_target(ssh_common libssh_wrapper)
if(MULTIPASS_ENABLE_TESTS)
  add_ssh_target(ssh_test libssh_wrapper_test)
endif()

function(add_sftp_client_target TARGET_NAME LIBSSH_TARGET)
  add_library(${TARGET_NAME} STATIC
    ssh_factory.cpp
    sftp_client.cpp
    sftp_dir_iterator.cpp
    sftp_utils.cpp
    plain_ssh_session.cpp)

  target_link_libraries(${TARGET_NAME}
    ${LIBSSH_TARGET}
    fmt::fmt-header-only
    ssh
    utils
    Qt6::Core)
endfunction()

add_sftp_client_target(sftp_client libssh_wrapper)
if(MULTIPASS_ENABLE_TESTS)
  add_sftp_client_target(sftp_test libssh_wrapper_test)
endif()

function(add_ssh_client_target TARGET_NAME LIBSSH_TARGET)
  add_library(${TARGET_NAME} STATIC
    ssh_factory.cpp
    ssh_client.cpp
    plain_ssh_session.cpp)

  target_link_libraries(${TARGET_NAME}
    ${LIBSSH_TARGET}
    console
    fmt::fmt-header-only
    ssh
    utils
    Qt6::Core)
endfunction()

add_ssh_client_target(ssh_client libssh_wrapper)
if(MULTIPASS_ENABLE_TESTS)
  add_ssh_client_target(ssh_client_test libssh_wrapper_test)
endif()
