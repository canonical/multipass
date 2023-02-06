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

target_sources(multipass_tests PRIVATE
  ${CMAKE_CURRENT_LIST_DIR}/mock_libc_functions.cpp
  ${CMAKE_CURRENT_LIST_DIR}/test_daemon_rpc.cpp
  ${CMAKE_CURRENT_LIST_DIR}/test_platform_unix.cpp
  ${CMAKE_CURRENT_LIST_DIR}/test_unix_terminal.cpp
)

target_compile_definitions(console_test PRIVATE
  -Disatty=ut_isatty
  -Dfileno=ut_fileno
  -Dtcgetattr=ut_tcgetattr
  -Dtcsetattr=ut_tcsetattr)

target_compile_definitions(platform_test PRIVATE
  -Dgetgrnam=ut_getgrnam
)

target_link_libraries(multipass_tests
  console_test
  platform_test
)
