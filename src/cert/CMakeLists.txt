# Copyright © 2018-2020 Canonical Ltd.
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

add_library(cert STATIC
  biomem.cpp
  client_cert_store.cpp
  ssl_cert_provider.cpp
)

target_include_directories(cert PRIVATE
  ${OPENSSL_INCLUDE_DIR})

foreach(flag -Wno-nested-anon-types -Wno-gnu -Wno-pedantic -Wno-ignored-qualifiers)
  check_cxx_compiler_flag(${flag} SUPPORTED)
  if(SUPPORTED)
    target_compile_options(cert PRIVATE ${flag})
  endif()
endforeach()

target_link_libraries(cert
  fmt
  utils
  OpenSSL::SSL
  OpenSSL::Crypto
  Qt5::Core
)
