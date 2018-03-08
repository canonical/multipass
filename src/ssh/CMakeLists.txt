# Copyright © 2017-2018 Canonical Ltd.
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
# Authored by: Alberto Aguirre <alberto.aguirre@canonical.com>

function(add_ssh_target TARGET_NAME)
  add_library(${TARGET_NAME} STATIC
    openssh_key_provider.cpp
    ssh_client_key_provider.cpp
    ssh_process.cpp
    ssh_session.cpp)

  target_link_libraries(${TARGET_NAME}
    libssh
    Qt5::Core)
endfunction()

add_ssh_target(ssh)
if(MULTIPASS_ENABLE_TESTS)
  add_ssh_target(ssh_test)
endif()

function(add_scp_client_target TARGET_NAME)
  add_library(${TARGET_NAME} STATIC
    scp_client.cpp
    ssh_session.cpp)

  target_link_libraries(${TARGET_NAME}
    libssh
    Qt5::Core)
endfunction()

add_scp_client_target(scp_client)
if(MULTIPASS_ENABLE_TESTS)
  add_scp_client_target(scp_test)
endif()

add_library(ssh_client STATIC
  ssh_client.cpp
  ssh_session.cpp)

target_link_libraries(ssh_client
  console
  libssh
  utils
  Qt5::Core)
