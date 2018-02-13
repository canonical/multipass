# Copyright © 2018 Canonical Ltd.
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

function(add_c_mocks)
  if (NOT ARGN)
    message(SEND_ERROR "Error: add_c_mocks() called without any functions to mock")
    return()
  endif ()

  foreach (MOCK ${ARGN})
    # The original symbol is replaced with a prefix known by premock
    list(APPEND c_mock_defines -D${MOCK}=ut_${MOCK})
  endforeach()
  set(c_mock_defines ${c_mock_defines} PARENT_SCOPE)
endfunction()

# See premock.hpp to see how to define and implement a mocked c function
add_c_mocks(
  ssh_new
  ssh_connect
  ssh_is_connected
  ssh_options_set
  ssh_userauth_publickey
  ssh_channel_new
  ssh_channel_open_session
  ssh_channel_request_exec
  ssh_channel_read_timeout
  ssh_channel_get_exit_status
  sftp_server_new
  sftp_free
  sftp_server_init
  sftp_reply_status
  sftp_reply_name
  sftp_reply_attr
  sftp_reply_data
  sftp_reply_names_add
  sftp_get_client_message
  sftp_client_message_free
)
