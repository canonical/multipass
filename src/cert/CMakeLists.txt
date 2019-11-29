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

add_library(cert
  biomem.cpp
  client_cert_store.cpp
  ssl_cert_provider.cpp
)

target_include_directories(cert PRIVATE
  ${CMAKE_SOURCE_DIR}/3rd-party/grpc/third_party/boringssl/include)

CHECK_CXX_COMPILER_FLAG("-Wno-nested-anon-types" COMPILER_SUPPORTS_NO_NESTED_ANON_TYPES)
CHECK_CXX_COMPILER_FLAG("-Wno-gnu" COMPILER_SUPPORTS_NO_GNU)
CHECK_CXX_COMPILER_FLAG("-Wno-pedantic" COMPILER_SUPPORTS_NO_PEDANTIC)
CHECK_CXX_COMPILER_FLAG("-Wno-ignored-qualifiers" COMPILER_SUPPORTS_NO_IGNORED_QUALIFIERS)

set(ssl_ignore_header_warning_flags "")
if (COMPILER_SUPPORTS_NO_NESTED_ANON_TYPES)
  string(APPEND ssl_ignore_header_warning_flags " -Wno-nested-anon-types ")
endif()

if (COMPILER_SUPPORTS_NO_GNU)
  string(APPEND ssl_ignore_header_warning_flags " -Wno-gnu ")
endif()

if (COMPILER_SUPPORTS_NO_PEDANTIC)
  string(APPEND ssl_ignore_header_warning_flags " -Wno-pedantic ")
endif()

if (COMPILER_SUPPORTS_NO_IGNORED_QUALIFIERS)
  string(APPEND ssl_ignore_header_warning_flags " -Wno-ignored-qualifiers ")
endif()

if (ssl_ignore_header_warning_flags)
  set_source_files_properties(ssl_cert_provider.cpp PROPERTIES COMPILE_FLAGS ${ssl_ignore_header_warning_flags})
  set_source_files_properties(client_cert_store.cpp PROPERTIES COMPILE_FLAGS ${ssl_ignore_header_warning_flags})
  set_source_files_properties(biomem.cpp PROPERTIES COMPILE_FLAGS ${ssl_ignore_header_warning_flags})
endif()

target_link_libraries(cert
  crypto
  fmt
  utils
  Qt5::Core
)
