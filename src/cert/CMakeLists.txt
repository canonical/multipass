# Copyright © 2018-2021 Canonical Ltd.
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

function(add_target TARGET_NAME)
  add_library(${TARGET_NAME} STATIC
    biomem.cpp
    client_cert_store.cpp
    passphrase_handler.cpp
    ssl_cert_provider.cpp
  )

  target_include_directories(${TARGET_NAME} PRIVATE
    ${OPENSSL_INCLUDE_DIR})

  foreach(flag -Wno-nested-anon-types -Wno-gnu -Wno-pedantic -Wno-ignored-qualifiers)
    check_cxx_compiler_flag(${flag} SUPPORTED)
    if(SUPPORTED)
      target_compile_options(${TARGET_NAME} PRIVATE ${flag})
    endif()
  endforeach()

  target_link_libraries(${TARGET_NAME}
    fmt
    utils
    OpenSSL::SSL
    OpenSSL::Crypto
    Qt5::Core
  )
endfunction()

add_target(cert)
if(MULTIPASS_ENABLE_TESTS)
  add_target(cert_test)
endif()
