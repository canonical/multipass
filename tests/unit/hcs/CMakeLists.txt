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


if(WIN32)
    target_sources(multipass_cpp_tests
    PRIVATE
        ${CMAKE_CURRENT_LIST_DIR}/test_ut_hcn_api.cpp
        ${CMAKE_CURRENT_LIST_DIR}/test_ut_hcn_route.cpp
        ${CMAKE_CURRENT_LIST_DIR}/test_ut_hcn_subnet.cpp
        ${CMAKE_CURRENT_LIST_DIR}/test_ut_hcn_ipam.cpp
        ${CMAKE_CURRENT_LIST_DIR}/test_ut_hcn_network_policy.cpp
        ${CMAKE_CURRENT_LIST_DIR}/test_ut_hcs_api.cpp
        ${CMAKE_CURRENT_LIST_DIR}/test_ut_hcs_request.cpp
        ${CMAKE_CURRENT_LIST_DIR}/test_ut_virtdisk.cpp
        ${CMAKE_CURRENT_LIST_DIR}/test_ut_virtdisk_snapshot.cpp
        ${CMAKE_CURRENT_LIST_DIR}/test_ut_hcs_virtual_machine.cpp
        ${CMAKE_CURRENT_LIST_DIR}/test_ut_hcs_virtual_machine_factory.cpp
    )

    target_sources(multipass_integration_tests
    PRIVATE
        ${CMAKE_CURRENT_LIST_DIR}/test_it_hcn_api.cpp
        ${CMAKE_CURRENT_LIST_DIR}/test_it_hcs_api.cpp
        ${CMAKE_CURRENT_LIST_DIR}/test_it_virtdisk.cpp
        ${CMAKE_CURRENT_LIST_DIR}/test_it_virtdisk_snapshot.cpp
        ${CMAKE_CURRENT_LIST_DIR}/test_bb_cit_hyperv.cpp
    )

    target_link_libraries(multipass_cpp_tests hcs_backend)
    target_link_libraries(multipass_integration_tests PRIVATE hcs_backend platform)
endif()
