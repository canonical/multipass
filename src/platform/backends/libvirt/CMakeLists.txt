#Copyright (C) Canonical, Ltd.
#
#This program is free software : you can redistribute it and / or modify
#it under the terms of the GNU General Public License version 3 as
#published by the Free Software Foundation.
#
#This program is distributed in the hope that it will be useful,
#but WITHOUT ANY WARRANTY; without even the implied warranty of
#MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.See the
#GNU General Public License for more details.
#
#You should have received a copy of the GNU General Public License
#along with this program.If not, see < http: // www.gnu.org/licenses/>.
#

find_package(PkgConfig)
pkg_check_modules(LIBVIRT libvirt REQUIRED)

function(add_libvirt_target TARGET_NAME)
  add_library(${TARGET_NAME} STATIC
    libvirt_virtual_machine_factory.cpp
    libvirt_virtual_machine.cpp
    libvirt_wrapper.cpp)

  target_include_directories(${TARGET_NAME} PRIVATE ${LIBVIRT_INCLUDE_DIRS})
  target_link_libraries(${TARGET_NAME}
    daemon
    fmt
    ip_address
    qemu_img_utils
    Qt6::Core
    shared_linux
    ssh
    utils
    yaml
    ${CMAKE_DL_LIBS})
endfunction()

add_libvirt_target(libvirt_backend)
if(MULTIPASS_ENABLE_TESTS)
  add_libvirt_target(libvirt_backend_test)
endif()
