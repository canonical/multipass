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

include(FindPkgConfig)
pkg_search_module(APPARMOR libapparmor REQUIRED)

find_package(Qt5 REQUIRED COMPONENTS DBus)

function(add_target TARGET_NAME)
  add_library(${TARGET_NAME} STATIC
    apparmor.cpp
    backend_utils.cpp
    process_factory.cpp)

  target_link_libraries(${TARGET_NAME}
    apparmor
    fmt
    scope_guard
    shared
    utils
    Qt5::Core
    Qt5::DBus)
endfunction()

add_target(shared_linux)
if(MULTIPASS_ENABLE_TESTS)
  add_target(shared_linux_test)
endif()

include_directories(shared_linux
  ..)

if (qemu IN_LIST MULTIPASS_BACKENDS OR libvirt IN_LIST MULTIPASS_BACKENDS)
  add_custom_target(copy-check-kvm-support ALL DEPENDS
    ${CMAKE_BINARY_DIR}/bin/check_kvm_support)
  add_custom_command(OUTPUT ${CMAKE_BINARY_DIR}/bin/check_kvm_support
    COMMAND ${CMAKE_COMMAND} -E copy
        ${CMAKE_CURRENT_SOURCE_DIR}/check_kvm_support
        ${CMAKE_BINARY_DIR}/bin/check_kvm_support
    DEPENDS ${CMAKE_CURRENT_SOURCE_DIR}/check_kvm_support)

  install(PROGRAMS check_kvm_support
    DESTINATION bin)
endif()
