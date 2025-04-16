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
    target_sources(multipass_tests
    PRIVATE
        ${CMAKE_CURRENT_LIST_DIR}/test_it_hyperv_hcn_api.cpp
        ${CMAKE_CURRENT_LIST_DIR}/test_ut_hyperv_hcn_api.cpp
        ${CMAKE_CURRENT_LIST_DIR}/test_ut_hyperv_hcn_route.cpp
        ${CMAKE_CURRENT_LIST_DIR}/test_ut_hyperv_hcn_subnet.cpp
        ${CMAKE_CURRENT_LIST_DIR}/test_ut_hyperv_hcn_ipam.cpp
        ${CMAKE_CURRENT_LIST_DIR}/test_ut_hyperv_hcn_network_policy.cpp
        ${CMAKE_CURRENT_LIST_DIR}/test_it_hyperv_hcs_api.cpp
        ${CMAKE_CURRENT_LIST_DIR}/test_ut_hyperv_hcs_api.cpp
        ${CMAKE_CURRENT_LIST_DIR}/test_it_hyperv_virtdisk.cpp
        ${CMAKE_CURRENT_LIST_DIR}/test_ut_hyperv_virtdisk.cpp
        ${CMAKE_CURRENT_LIST_DIR}/test_bb_cit_hyperv.cpp
    )
endif()
